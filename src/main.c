/* root/src/main.c - BLE NUS → Servo bridge */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdbool.h>
#include <zephyr/drivers/gpio.h>
#include <bluetooth/services/mds.h>


#include "adc_demo.h"
#include "imu.h"
#include "madgwick.h"
#include "servo.h"
#include "madgwick_motion.h"
#include "servo_logic.h"
#include "ble_nus.h"


/* ---- Posture detection threshold ---- */
#define POSTURE_ANGLE_THRESHOLD 50.0f   /* 超过 15° 认为姿势不良 */
#define PRESSURE_LOWER_LIMIT     2000    

/* ---- Main loop interval ---- */
#define LOOP_INTERVAL_MS         2000   /* 2000 ms 更新一次 */

/* BLE basic setting */
#define DEVICE_NAME      "PostureCare"
#define DEVICE_NAME_LEN  (sizeof(DEVICE_NAME) - 1)

LOG_MODULE_REGISTER(root_main, LOG_LEVEL_INF);

#define VIB_PORT_NODE  DT_NODELABEL(gpio1)   /* nRF 的 P1.xx 对应 gpio1 */
#define VIB_PIN1       8                     /* P1.08 */
#define VIB_PIN2       9                     /* P1.09 */

static const struct device *vib_port = DEVICE_DT_GET(VIB_PORT_NODE);



static void vibrate_warning(void)
{
    /* 高电平启动振动马达（如果你用的是低有效，就反着写） */
    gpio_pin_set(vib_port, VIB_PIN1, 1);
    gpio_pin_set(vib_port, VIB_PIN2, 1);

    k_sleep(K_SECONDS(2));

    gpio_pin_set(vib_port, VIB_PIN1, 0);
    gpio_pin_set(vib_port, VIB_PIN2, 0);
}


/* ====================== NUS RX → Servo 命令 ====================== */

static void root_rx_cb(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        char cmd = (char)data[i];

        LOG_INF("RX cmd '%c' (0x%02x)",
                (cmd >= 32 && cmd <= 126) ? cmd : '.',
                (unsigned char)cmd);

        servo_apply_command(cmd);
    }
}

/* ====================== NUS TX 发送线程 ====================== */

K_MSGQ_DEFINE(ble_tx_queue, 32, 50, 4);  /* msg size=32 bytes, 50 messages */

void ble_tx_thread(void)
{
    char msg[32];

    while (1) {
        /* Wait for a message */
        k_msgq_get(&ble_tx_queue, msg, K_FOREVER);

        /* Attempt to send (handle ENOMEM/EAGAIN) */
        int err;
        do {
            err = bt_nus_send(NULL, msg, strlen(msg));
            if (err == -ENOMEM || err == -EAGAIN) {
                k_msleep(50);  /* small retry delay */
            }
        } while (err == -ENOMEM || err == -EAGAIN);

        if (err) {
            printk("BLE TX error: %d\n", err);
            k_msleep(50);
        }
    }
}

K_THREAD_DEFINE(ble_tx_id,
                2048,
                ble_tx_thread,
                NULL, NULL, NULL,
                7, 0, 0);

/* ====================== main ====================== */

void main(void)
{
    LOG_INF("System start...");

    int err;

     /* ============ Register MDS callbacks BEFORE BLE init ============ */
    static const struct bt_mds_cb mds_cb = {
        .access_enable = NULL,  // Or implement access control if needed
    };

    err = bt_mds_cb_register(&mds_cb);
    if (err) {
        LOG_ERR("MDS callback registration failed: %d", err);
        return;
    }
    LOG_INF("MDS callbacks registered");

    /* ---- initialize BLE, and register RX callback ---- */
    err = ble_nus_run(root_rx_cb);
    if (err) {
        LOG_ERR("BLE init failed: %d", err);
        return;
    }
    LOG_INF("BLE ready");

    /* ------------ initialize IMUs (two sensors) ------------ */
    if (imu_init() != 0) {
        LOG_ERR("IMU init failed!");
        return;
    }
    LOG_INF("IMU0 & IMU1 OK");

    /* ------------ initialize ADC ------------ */
    if (adc_demo_init() != 0) {
        LOG_ERR("ADC init failed!");
        return;
    }
    LOG_INF("ADC OK");

    /* ------------ initialize Servo ------------ */
    if (servo_init() != 0) {
        LOG_ERR("Servo init failed!");
        return;
    }
    LOG_INF("Servo OK and centered");

    /* ------------ initialize vibration motors GPIO ------------ */
    if (!device_is_ready(vib_port)) {
        LOG_ERR("Vibration GPIO not ready");
        return;
    }

    gpio_pin_configure(vib_port, VIB_PIN1,
                       GPIO_OUTPUT_INACTIVE | GPIO_ACTIVE_HIGH);
    gpio_pin_configure(vib_port, VIB_PIN2,
                       GPIO_OUTPUT_INACTIVE | GPIO_ACTIVE_HIGH);



    /* ========= IMU0 / IMU1 参考姿态 & 计数 ========= */
    struct imu_angles imu0_ref = {0};
    struct imu_angles imu1_ref = {0};
    bool imu0_ref_set = false;
    bool imu1_ref_set = false;
    int  imu0_sample_count = 0;
    int  imu1_sample_count = 0;

    /* ========= 上一周期的状态，用于检测变化（可选） ========= */
    bool prev_bad_posture0 = false;
    bool prev_bad_posture1 = false;
    bool prev_low_pressure1 = false;
    bool prev_low_pressure2 = false;

    uint32_t loop_cnt = 0;

    /* ------------ main loop ------------ */
    while (1) {

        /* ------- loop 头：给循环之间一个大间隔 ------- */
        printk("\n\n========== LOOP %lu ==========\n",
               (unsigned long)loop_cnt++);

        /* ===== IMU0: 主传感器 ===== */
        struct imu_angles imu0_abs = {0};   /* IMU0 绝对角 */
        struct imu_angles imu0_rel = {0};   /* IMU0 相对角 */
        bool imu0_rel_valid = false;
        bool bad_posture0   = false;

        err = imu_update_main(&imu0_abs);
        if (err != 0) {
            LOG_WRN("imu_update_main() not ready, err=%d", err);
        } else {
            imu0_sample_count++;

            /* 第 5 次：锁定参考姿态（只打一次 log） */
            if (!imu0_ref_set && imu0_sample_count == 5) {
                imu0_ref     = imu0_abs;
                imu0_ref_set = true;

                LOG_INF("IMU0 reference locked: R0=%.2f, P0=%.2f, Y0=%.2f",
                        imu0_ref.roll, imu0_ref.pitch, imu0_ref.yaw);
            }

            /* 有了参考姿态才计算相对角和姿态判断，并且每次循环打印 */
            if (imu0_ref_set) {
                imu0_rel.roll  = imu0_abs.roll  - imu0_ref.roll;
                imu0_rel.pitch = imu0_abs.pitch - imu0_ref.pitch;
                imu0_rel.yaw   = imu0_abs.yaw   - imu0_ref.yaw;
                imu0_rel_valid = true;

                bad_posture0 =
                    (fabsf(imu0_rel.roll)  > POSTURE_ANGLE_THRESHOLD) ||
                    (fabsf(imu0_rel.pitch) > POSTURE_ANGLE_THRESHOLD);

                printk("IMU0 ANGLES (relative):\n");
                printk("  ROLL0  = %.2f deg\n", imu0_rel.roll);
                printk("  PITCH0 = %.2f deg\n", imu0_rel.pitch);
                printk("  YAW0   = %.2f deg\n", imu0_rel.yaw);
                printk("  STATUS0= %s\n", bad_posture0 ? "BAD" : "OK");
            }
        }

        /* IMU0 和 IMU1 之间插一行空行，视觉上分开 */
        printk("\n");

        /* ===== IMU1: 第二个传感器 ===== */
        struct imu_angles imu1_abs = {0};   /* IMU1 绝对角 */
        struct imu_angles imu1_rel = {0};   /* IMU1 相对角 */
        bool imu1_rel_valid = false;
        bool bad_posture1   = false;

        int err_motion = imu_update_motion(&imu1_abs);
        if (err_motion != 0) {
            LOG_WRN("imu_update_motion() not ready, err=%d", err_motion);
        } else {
            imu1_sample_count++;

            if (!imu1_ref_set && imu1_sample_count == 5) {
                imu1_ref     = imu1_abs;
                imu1_ref_set = true;

                LOG_INF("IMU1 reference locked: R1=%.2f, P1=%.2f, Y1=%.2f",
                        imu1_ref.roll, imu1_ref.pitch, imu1_ref.yaw);
            }

            if (imu1_ref_set) {
                imu1_rel.roll  = imu1_abs.roll  - imu1_ref.roll;
                imu1_rel.pitch = imu1_abs.pitch - imu1_ref.pitch;
                imu1_rel.yaw   = imu1_abs.yaw   - imu1_ref.yaw;
                imu1_rel_valid = true;

                bad_posture1 =
                    (fabsf(imu1_rel.roll)  > POSTURE_ANGLE_THRESHOLD) ||
                    (fabsf(imu1_rel.pitch) > POSTURE_ANGLE_THRESHOLD);

                printk("IMU1 ANGLES (relative):\n");
                printk("  ROLL1  = %.2f deg\n", imu1_rel.roll);
                printk("  PITCH1 = %.2f deg\n", imu1_rel.pitch);
                printk("  YAW1   = %.2f deg\n", imu1_rel.yaw);
                printk("  STATUS1= %s\n", bad_posture1 ? "BAD" : "OK");
            }
        }

              /* ===== 压力传感器 1 & 2 ===== */
        int pressure1 = adc_demo_read();    /* existing sensor on P0.06 / AIN2 */
        int pressure2 = adc_demo_read2();   /* NEW sensor on P0.07 / AIN3 */

        bool low_pressure1 = false;
        bool low_pressure2 = false;

        if (pressure1 < 0) {
            LOG_ERR("ADC read error (sensor1): %d", pressure1);
        } else {
            low_pressure1 = (pressure1 > PRESSURE_LOWER_LIMIT);
            printk("\nPressure1 = %d  (%s)\n",
                   pressure1,
                   low_pressure1 ? "High" : "OK");
        }

        if (pressure2 < 0) {
            LOG_ERR("ADC read error (sensor2): %d", pressure2);
        } else {
            low_pressure2 = (pressure2 > PRESSURE_LOWER_LIMIT);
            printk("Pressure2 = %d  (%s)\n",
                   pressure2,
                   low_pressure2 ? "High" : "OK");
        }

        bool low_pressure = low_pressure1 || low_pressure2;  /* keep a combined flag */


        /* ===== 状态变化时再用 LOG_INF 打高层信息 ===== */
        bool bad_posture = bad_posture0 || bad_posture1;

        if ((bad_posture0 != prev_bad_posture0) ||
            (bad_posture1 != prev_bad_posture1)) {

            if (bad_posture) {
                LOG_INF("Bad posture: IMU0[%s] IMU1[%s]",
                        bad_posture0 ? "BAD" : "OK",
                        bad_posture1 ? "BAD" : "OK");
            } else {
                LOG_INF("Posture corrected: IMU0 & IMU1 both OK");
            }
        }

         if (low_pressure1 != prev_low_pressure1) {
            LOG_INF("Pressure1=%d → %s",
                    pressure1,
                    low_pressure1 ? "high (loosen)" : "OK");
        }

        if (low_pressure2 != prev_low_pressure2) {
            LOG_INF("Pressure2=%d → %s",
                    pressure2,
                    low_pressure2 ? "high (loosen)" : "OK");
        }

        prev_bad_posture0 = bad_posture0;
        prev_bad_posture1 = bad_posture1;
        prev_low_pressure1 = low_pressure1;
        prev_low_pressure2 = low_pressure2;

        /* ===== 舵机控制：放在所有打印之后 ===== */
        if (bad_posture) {
            vibrate_warning();
            servo_apply_command('1');   /* tighten */
            k_sleep(K_MSEC(200));
        }

        if (low_pressure) {
            servo_apply_command('2');   /* loosen */
            k_sleep(K_MSEC(200));
        }

        /* --------------------------------------------------
         *  sending data to phone via BLE
         * -------------------------------------------------- */
        if (ble_nus_ready()
            && imu0_ref_set && imu1_ref_set
            && imu0_rel_valid && imu1_rel_valid) {

            char msg[32];

            /* Pressure */
            snprintf(msg, sizeof(msg), "Pressure=%d", pressure1);
            k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
            k_msleep(50);

            snprintf(msg, sizeof(msg), "Pressure2=%d", pressure2);
            k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
            k_msleep(50);

            /* IMU0: relative angles */
            snprintf(msg, sizeof(msg), "Roll0=%.1f", imu0_rel.roll);
            k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
            k_msleep(50);

            snprintf(msg, sizeof(msg), "Pitch0=%.1f", imu0_rel.pitch);
            k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
            k_msleep(50);

            snprintf(msg, sizeof(msg), "Yaw0=%.1f", imu0_rel.yaw);
            k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
            k_msleep(50);

            /* IMU1: relative angles */
            snprintf(msg, sizeof(msg), "Roll1=%.1f", imu1_rel.roll);
            k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
            k_msleep(50);

            snprintf(msg, sizeof(msg), "Pitch1=%.1f", imu1_rel.pitch);
            k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
            k_msleep(50);

            snprintf(msg, sizeof(msg), "Yaw1=%.1f", imu1_rel.yaw);
            k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
            k_msleep(50);
        }

        /* -------- 5. 延时：循环之间物理时间也隔开 -------- */
        k_sleep(K_MSEC(LOOP_INTERVAL_MS));
    }
}