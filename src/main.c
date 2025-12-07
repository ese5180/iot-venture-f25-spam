/* root/src/main.c - BLE NUS → Servo bridge */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdbool.h>

#include "adc_demo.h"
#include "imu.h"
#include "madgwick.h"
#include "servo.h"
#include "madgwick_motion.h"
#include "servo_logic.h"
#include "ble_nus.h"

// /* ---- Posture detection threshold---- */
// #define POSTURE_ANGLE_THRESHOLD 15.0f // 超过 5° 认为姿势不良
// #define PRESSURE_LOWER_LIMIT 100 // 压力过低 → 说明绑带太紧需要放松

// /* ---- Main loop interval ---- */
// #define LOOP_INTERVAL_MS 2000 // 2000 ms 更新一次

// /* BLE basic setting */
// #define DEVICE_NAME "PostureCare"
// #define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

// LOG_MODULE_REGISTER(root_main, LOG_LEVEL_INF);

// static void root_rx_cb(const uint8_t *data, size_t len) {
//   for (size_t i = 0; i < len; i++) {
//     char cmd = (char)data[i];

//     LOG_INF("RX cmd '%c' (0x%02x)", (cmd >= 32 && cmd <= 126) ? cmd : '.',
//             (unsigned char)cmd);

//     servo_apply_command(cmd);
//   }
// }

// K_MSGQ_DEFINE(ble_tx_queue, 32, 50, 4);  // msg size=32 bytes, 10 messages


// void ble_tx_thread(void)
// {
//     char msg[32];

//     while (1) {
//         /* Wait for a message */
//         k_msgq_get(&ble_tx_queue, msg, K_FOREVER);

//         /* Attempt to send (handle ENOMEM/EAGAIN) */
//         int err;
//         do {
//             err = bt_nus_send(NULL, msg, strlen(msg));
//             if (err == -ENOMEM || err == -EAGAIN) {
//                 k_msleep(50);  // small retry delay
//             }
//         } while (err == -ENOMEM || err == -EAGAIN);

//         if (err) {
//             printk("BLE TX error: %d\n", err);
//              k_msleep(50);  
//         }
//     }
// }

// K_THREAD_DEFINE(ble_tx_id, 2048, ble_tx_thread, NULL, NULL, NULL,7, 0, 0);



// void main(void) {
//     LOG_INF("System start...");

//     int err;

//     /* ---- initialize BLE，and register RX callback ---- */
//     err = ble_nus_run(root_rx_cb);
//     if (err) {
//         LOG_ERR("BLE init failed: %d", err);
//         return;
//     }
//     LOG_INF("BLE ready");

//     /* ------------ initialize IMU ------------ */
//     if (imu_init() != 0) {
//         LOG_ERR("IMU init failed!");
//         return;
//     }
//     LOG_INF("IMU OK");

//     /* ------------ initialize ADC ------------ */
//     if (adc_demo_init() != 0) {
//         LOG_ERR("ADC init failed!");
//         return;
//     }
//     LOG_INF("ADC OK");

//     /* ------------ initialize Servo ------------ */
//     if (servo_init() != 0) {
//         LOG_ERR("Servo init failed!");
//         return;
//     }
//     LOG_INF("Servo OK and centered");

//     /* ========= posture standard ========= */
//     struct imu_angles imu_ref = {0};
//     bool imu_ref_set = false;
//     static int imu_sample_count = 0;   

//     /* ------------ main loop ------------ */
//     while (1) {

//         struct imu_angles angles;      // 绝对角
//         struct imu_angles rel;         // 相对角
//         bool rel_valid = false;        // 这一轮是否有相对角可用

//         /* -------- 1. 读取姿态角（绝对） -------- */
//         err = imu_update(&angles);
//         if (err != 0) {
//             LOG_WRN("imu_update() not ready, err=%d", err);
//         } else {
//             imu_sample_count++;

//             /* 第 1~4 次：只让滤波器稳定，不做任何姿势判断 */
//             if (!imu_ref_set && imu_sample_count < 5) {
//                 LOG_INF("IMU warm-up sample %d: R=%.2f P=%.2f Y=%.2f",
//                         imu_sample_count,
//                         angles.roll, angles.pitch, angles.yaw);
//             }
//             /* 第 5 次：锁定参考姿态 */
//             else if (!imu_ref_set && imu_sample_count == 5) {
//                 imu_ref = angles;
//                 imu_ref_set = true;

//                 LOG_INF("IMU reference locked (5th sample): "
//                         "R0=%.2f, P0=%.2f, Y0=%.2f",
//                         imu_ref.roll, imu_ref.pitch, imu_ref.yaw);
//             }

//             /* 参考姿态已经锁定 → 计算相对角，做姿势判断 */
//             if (imu_ref_set) {
//                 rel.roll  = angles.roll  - imu_ref.roll;
//                 rel.pitch = angles.pitch - imu_ref.pitch;
//                 rel.yaw   = angles.yaw   - imu_ref.yaw;
//                 rel_valid = true;

//                 printk("\n");
//                 printk("IMU ANGLES (relative to reference):\n");
//                 printk("  ROLL = %.2f deg\n",  rel.roll);
//                 printk("  PITCH= %.2f deg\n",  rel.pitch);
//                 printk("  YAW  = %.2f deg\n",  rel.yaw);

//                 /* -------- 2. 坏姿势检测（用相对角度） -------- */
//                 bool bad_posture =
//                     (fabsf(rel.roll)  > POSTURE_ANGLE_THRESHOLD) ||
//                     (fabsf(rel.pitch) > POSTURE_ANGLE_THRESHOLD);

//                 if (bad_posture) {
//                     LOG_INF("Bad posture detected → servo tighten");
//                     servo_apply_command('1'); // tighten
//                     k_sleep(K_MSEC(200));
//                 } else {
//                     LOG_INF("Posture OK (R=%.2f, P=%.2f)",
//                             rel.roll, rel.pitch);
//                 }
//             }
//         }

//         /* -------- 3. 读取压力传感器 -------- */
//         int pressure = adc_demo_read();
//         if (pressure < 0) {
//             LOG_ERR("ADC read error: %d", pressure);
//         } else {
//             LOG_INF("Pressure = %d", pressure);
//         }

//         /* -------- 4. 压力过低 → 放松（发“2”命令） -------- */
//         if (pressure < PRESSURE_LOWER_LIMIT) {
//             LOG_INF("Pressure low → servo loosen");
//             servo_apply_command('2'); // loosen
//             k_sleep(K_MSEC(200));
//         }

//         printk("\n"); // 打印一行空行
//         LOG_INF(" ");

//         /* --------------------------------------------------
//          *  sending data to phone via ble
//          * -------------------------------------------------- */
//         if (ble_nus_ready() && imu_ref_set && rel_valid) {

//             char msg[32];

//             /* Send pressure */
//             snprintf(msg, sizeof(msg), "Pressure=%d", pressure);
//             k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
//             k_msleep(50);   // cool-down

//             /* Send roll (relative) */
//             snprintf(msg, sizeof(msg), "Roll=%.1f", rel.roll);
//             k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
//             k_msleep(50);

//             /* Send pitch (relative) */
//             snprintf(msg, sizeof(msg), "Pitch=%.1f", rel.pitch);
//             k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
//             k_msleep(50);

//             /* Send yaw (relative) */
//             snprintf(msg, sizeof(msg), "YAW=%.1f", rel.yaw);
//             k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
//             k_msleep(50);
//         }

//         /* -------- 5. 延时 -------- */
//         k_sleep(K_MSEC(LOOP_INTERVAL_MS));
//     }
// }



// /* ---- Posture detection threshold ---- */
// #define POSTURE_ANGLE_THRESHOLD 15.0f   /* 超过 15° 认为姿势不良 */
// #define PRESSURE_LOWER_LIMIT     100    /* 压力过低 → 说明绑带太松，需要收紧 */

// /* ---- Main loop interval ---- */
// #define LOOP_INTERVAL_MS         2000   /* 2000 ms 更新一次 */

// /* BLE basic setting */
// #define DEVICE_NAME      "PostureCare"
// #define DEVICE_NAME_LEN  (sizeof(DEVICE_NAME) - 1)

// LOG_MODULE_REGISTER(root_main, LOG_LEVEL_INF);

// /* ====================== NUS RX → Servo 命令 ====================== */

// static void root_rx_cb(const uint8_t *data, size_t len)
// {
//     for (size_t i = 0; i < len; i++) {
//         char cmd = (char)data[i];

//         LOG_INF("RX cmd '%c' (0x%02x)",
//                 (cmd >= 32 && cmd <= 126) ? cmd : '.',
//                 (unsigned char)cmd);

//         servo_apply_command(cmd);
//     }
// }

// /* ====================== NUS TX 发送线程 ====================== */

// K_MSGQ_DEFINE(ble_tx_queue, 32, 50, 4);  /* msg size=32 bytes, 50 messages */

// void ble_tx_thread(void)
// {
//     char msg[32];

//     while (1) {
//         /* Wait for a message */
//         k_msgq_get(&ble_tx_queue, msg, K_FOREVER);

//         /* Attempt to send (handle ENOMEM/EAGAIN) */
//         int err;
//         do {
//             err = bt_nus_send(NULL, msg, strlen(msg));
//             if (err == -ENOMEM || err == -EAGAIN) {
//                 k_msleep(50);  /* small retry delay */
//             }
//         } while (err == -ENOMEM || err == -EAGAIN);

//         if (err) {
//             printk("BLE TX error: %d\n", err);
//             k_msleep(50);
//         }
//     }
// }

// K_THREAD_DEFINE(ble_tx_id,
//                 2048,
//                 ble_tx_thread,
//                 NULL, NULL, NULL,
//                 7, 0, 0);

// /* ====================== main ====================== */

// void main(void)
// {
//     LOG_INF("System start...");

//     int err;

//     /* ---- initialize BLE, and register RX callback ---- */
//     err = ble_nus_run(root_rx_cb);
//     if (err) {
//         LOG_ERR("BLE init failed: %d", err);
//         return;
//     }
//     LOG_INF("BLE ready");

//     /* ------------ initialize IMUs (two sensors) ------------ */
//     if (imu_init() != 0) {
//         LOG_ERR("IMU init failed!");
//         return;
//     }
//     LOG_INF("IMU0 & IMU1 OK");

//     /* ------------ initialize ADC ------------ */
//     if (adc_demo_init() != 0) {
//         LOG_ERR("ADC init failed!");
//         return;
//     }
//     LOG_INF("ADC OK");

//     /* ------------ initialize Servo ------------ */
//     if (servo_init() != 0) {
//         LOG_ERR("Servo init failed!");
//         return;
//     }
//     LOG_INF("Servo OK and centered");

//     /* ========= 主 IMU（IMU0）参考姿态 ========= */
//     struct imu_angles imu_ref = {0};
//     bool imu_ref_set = false;
//     static int imu_sample_count = 0;

//     /* 第二个 IMU（IMU1）的姿态（先只当成绝对角输出用） */
//     struct imu_angles motion_angles = {0};

//     /* ------------ main loop ------------ */
//     while (1) {

//         struct imu_angles angles;   /* 主 IMU 的绝对角 */
//         struct imu_angles rel;      /* 主 IMU 的相对角 */
//         bool rel_valid = false;     /* 这一轮是否有相对角可用 */

//         /* -------- 1. 读取主 IMU 姿态角（绝对） -------- */
//         err = imu_update_main(&angles);
//         if (err != 0) {
//             LOG_WRN("imu_update_main() not ready, err=%d", err);
//         } else {
//             imu_sample_count++;

//             /* 第 1~4 次：只让滤波器稳定，不做任何姿势判断 */
//             if (!imu_ref_set && imu_sample_count < 5) {
//                 LOG_INF("IMU0 warm-up sample %d: R=%.2f P=%.2f Y=%.2f",
//                         imu_sample_count,
//                         angles.roll, angles.pitch, angles.yaw);
//             }
//             /* 第 5 次：锁定参考姿态 */
//             else if (!imu_ref_set && imu_sample_count == 5) {
//                 imu_ref = angles;
//                 imu_ref_set = true;

//                 LOG_INF("IMU0 reference locked (5th sample): "
//                         "R0=%.2f, P0=%.2f, Y0=%.2f",
//                         imu_ref.roll, imu_ref.pitch, imu_ref.yaw);
//             }

//             /* 参考姿态已经锁定 → 计算相对角，做姿势判断 */
//             if (imu_ref_set) {
//                 rel.roll  = angles.roll  - imu_ref.roll;
//                 rel.pitch = angles.pitch - imu_ref.pitch;
//                 rel.yaw   = angles.yaw   - imu_ref.yaw;
//                 rel_valid = true;

//                 printk("\n");
//                 printk("IMU0 ANGLES (relative to reference):\n");
//                 printk("  ROLL0 = %.2f deg\n",  rel.roll);
//                 printk("  PITCH0= %.2f deg\n",  rel.pitch);
//                 printk("  YAW0  = %.2f deg\n",  rel.yaw);

//                 /* -------- 2. 坏姿势检测（用相对角度） -------- */
//                 bool bad_posture =
//                     (fabsf(rel.roll)  > POSTURE_ANGLE_THRESHOLD) ||
//                     (fabsf(rel.pitch) > POSTURE_ANGLE_THRESHOLD);

//                 if (bad_posture) {
//                     LOG_INF("Bad posture detected → servo tighten");
//                     servo_apply_command('1'); /* tighten */
//                     k_sleep(K_MSEC(200));
//                 } else {
//                     LOG_INF("Posture OK (R=%.2f, P=%.2f)",
//                             rel.roll, rel.pitch);
//                 }
//             }
//         }

//         /* -------- 1b. 读取第二个 IMU（IMU1）姿态角（绝对） -------- */
//         int err_motion = imu_update_motion(&motion_angles);
//         if (err_motion != 0) {
//             LOG_WRN("imu_update_motion() not ready, err=%d", err_motion);
//         } else {
//             LOG_INF("IMU1 absolute: R1=%.2f, P1=%.2f, Y1=%.2f",
//                     motion_angles.roll,
//                     motion_angles.pitch,
//                     motion_angles.yaw);
//         }

//         /* -------- 3. 读取压力传感器 -------- */
//         int pressure = adc_demo_read();
//         if (pressure < 0) {
//             LOG_ERR("ADC read error: %d", pressure);
//         } else {
//             LOG_INF("Pressure = %d", pressure);
//         }

//         /* -------- 4. 压力过低 → 放松（发“2”命令） -------- */
//         if (pressure < PRESSURE_LOWER_LIMIT) {
//             LOG_INF("Pressure low → servo loosen");
//             servo_apply_command('2'); /* loosen */
//             k_sleep(K_MSEC(200));
//         }

//         printk("\n");
//         LOG_INF(" ");

//         /* --------------------------------------------------
//          *  sending data to phone via BLE
//          * -------------------------------------------------- */
//         if (ble_nus_ready() && imu_ref_set && rel_valid) {

//             char msg[32];

//             /* Send pressure */
//             snprintf(msg, sizeof(msg), "Pressure=%d", pressure);
//             k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
//             k_msleep(50);   /* cool-down */

//             /* 主 IMU：相对角（基于参考姿态） */
//             snprintf(msg, sizeof(msg), "Roll0=%.1f", rel.roll);
//             k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
//             k_msleep(50);

//             snprintf(msg, sizeof(msg), "Pitch0=%.1f", rel.pitch);
//             k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
//             k_msleep(50);

//             snprintf(msg, sizeof(msg), "Yaw0=%.1f", rel.yaw);
//             k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
//             k_msleep(50);

//             /* 第二个 IMU：绝对角（先纯显示） */
//             snprintf(msg, sizeof(msg), "Roll1=%.1f", motion_angles.roll);
//             k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
//             k_msleep(50);

//             snprintf(msg, sizeof(msg), "Pitch1=%.1f", motion_angles.pitch);
//             k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
//             k_msleep(50);

//             snprintf(msg, sizeof(msg), "Yaw1=%.1f", motion_angles.yaw);
//             k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
//             k_msleep(50);
//         }

//         /* -------- 5. 延时 -------- */
//         k_sleep(K_MSEC(LOOP_INTERVAL_MS));
//     }
// }



/* ---- Posture detection threshold ---- */
#define POSTURE_ANGLE_THRESHOLD 15.0f   /* 超过 15° 认为姿势不良 */
#define PRESSURE_LOWER_LIMIT     100    /* 压力过低 → 说明绑带太松，需要收紧 */

/* ---- Main loop interval ---- */
#define LOOP_INTERVAL_MS         2000   /* 2000 ms 更新一次 */

/* BLE basic setting */
#define DEVICE_NAME      "PostureCare"
#define DEVICE_NAME_LEN  (sizeof(DEVICE_NAME) - 1)

LOG_MODULE_REGISTER(root_main, LOG_LEVEL_INF);

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
    bool prev_low_pressure = false;

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

        /* ===== 压力传感器 ===== */
        int pressure = adc_demo_read();
        bool low_pressure = false;

        if (pressure < 0) {
            LOG_ERR("ADC read error: %d", pressure);
        } else {
            low_pressure = (pressure < PRESSURE_LOWER_LIMIT);
            printk("\nPressure = %d  (%s)\n",
                   pressure,
                   low_pressure ? "LOW" : "OK");
        }

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

        if (low_pressure != prev_low_pressure) {
            LOG_INF("Pressure=%d → %s",
                    pressure,
                    low_pressure ? "LOW (loosen)" : "OK");
        }

        prev_bad_posture0 = bad_posture0;
        prev_bad_posture1 = bad_posture1;
        prev_low_pressure = low_pressure;

        /* ===== 舵机控制：放在所有打印之后 ===== */
        if (bad_posture) {
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
            snprintf(msg, sizeof(msg), "Pressure=%d", pressure);
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