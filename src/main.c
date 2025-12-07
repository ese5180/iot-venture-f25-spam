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
#include "servo_logic.h"
#include "ble_nus.h"

/* ---- Posture detection threshold---- */
#define POSTURE_ANGLE_THRESHOLD 15.0f // 超过 5° 认为姿势不良
#define PRESSURE_LOWER_LIMIT 100 // 压力过低 → 说明绑带太紧需要放松

/* ---- Main loop interval ---- */
#define LOOP_INTERVAL_MS 2000 // 2000 ms 更新一次

/* BLE basic setting */
#define DEVICE_NAME "PostureCare"
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

LOG_MODULE_REGISTER(root_main, LOG_LEVEL_INF);

static void root_rx_cb(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    char cmd = (char)data[i];

    LOG_INF("RX cmd '%c' (0x%02x)", (cmd >= 32 && cmd <= 126) ? cmd : '.',
            (unsigned char)cmd);

    servo_apply_command(cmd);
  }
}

K_MSGQ_DEFINE(ble_tx_queue, 32, 50, 4);  // msg size=32 bytes, 10 messages


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
                k_msleep(50);  // small retry delay
            }
        } while (err == -ENOMEM || err == -EAGAIN);

        if (err) {
            printk("BLE TX error: %d\n", err);
             k_msleep(50);  
        }
    }
}

K_THREAD_DEFINE(ble_tx_id, 2048, ble_tx_thread, NULL, NULL, NULL,7, 0, 0);


// void main(void) {
//   LOG_INF("System start...");

//   int err;
//   /* ---- 初始化 BLE，并注册 RX 回调 ---- */
//   err = ble_nus_run(root_rx_cb);
//   if (err) {
//     LOG_ERR("BLE init failed: %d", err);
//     return;
//   }
//   LOG_INF("BLE ready");

//   /* ------------ 初始化 IMU ------------ */
//   if (imu_init() != 0) {
//     LOG_ERR("IMU init failed!");
//     return;
//   }
//   LOG_INF("IMU OK");

//   /* ------------ 初始化 ADC ------------ */
//   if (adc_demo_init() != 0) {
//     LOG_ERR("ADC init failed!");
//     return;
//   }
//   LOG_INF("ADC OK");

//   /* ------------ 初始化 Servo ------------ */
//   if (servo_init() != 0) {
//     LOG_ERR("Servo init failed!");
//     return;
//   }
//   LOG_INF("Servo OK and centered");

//   /* ------------ 主循环 ------------ */
//   while (1) {
//     /* -------- 1. 读取姿态角 -------- */
//     struct imu_angles angles;
//     err = imu_update(&angles);
//         if (err != 0) {
//             LOG_WRN("imu_update() not ready, err=%d", err);
//         } else {
//             printk("\n");
//             printk("IMU ANGLES (relative to reference):\n");
//             printk("  ROLL = %.2f deg\n",  angles.roll);
//             printk("  PITCH= %.2f deg\n",  angles.pitch);
//             printk("  YAW  = %.2f deg\n",  angles.yaw);

//             /* 可选：简单做一下坏姿势检测，只看 roll/pitch */
//             bool bad_posture =
//                 (fabsf(angles.roll)  > POSTURE_ANGLE_THRESHOLD) ||
//                 (fabsf(angles.pitch) > POSTURE_ANGLE_THRESHOLD);

//             if (bad_posture) {
//                 LOG_INF("Bad posture detected → servo tighten");
//                   servo_apply_command('1'); // tighten
//                   k_sleep(K_MSEC(200));
//             } else {
//                 LOG_INF("Posture OK (R=%.2f, P=%.2f)",
//                         angles.roll, angles.pitch);
//             }
//         }

//     /* -------- 3. 读取压力传感器 -------- */
//     int pressure = adc_demo_read();
//     if (pressure < 0) {
//       LOG_ERR("ADC read error: %d", pressure);
//     } else {
//       LOG_INF("Pressure = %d", pressure);
//     }

//     /* -------- 4. 压力过低 → 放松（发“2”命令） -------- */
//     if (pressure < PRESSURE_LOWER_LIMIT) {
//       LOG_INF("Pressure low → servo loosen");
//       servo_apply_command('2'); // loosen
//       k_sleep(K_MSEC(200));
//     }

//     printk("\n"); // 打印一行空行
//     LOG_INF(" ");

//     /* --------------------------------------------------
//      *  sending data to phone via ble
//      * -------------------------------------------------- */

//         if (ble_nus_ready()) {

//         char msg[32];

//         /* Send pressure */
//         snprintf(msg, sizeof(msg), "Pressure=%d", pressure);
//         k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
//         k_msleep(50);   // cool-down

//         /* Send roll */
//         snprintf(msg, sizeof(msg), "Roll=%.1f", angles.roll);
//         k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
//         k_msleep(50);

//         /* Send pitch */
//         snprintf(msg, sizeof(msg), "Pitch=%.1f", angles.pitch);
//         k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
//         k_msleep(50);

//         /* Send yaw */
//         snprintf(msg, sizeof(msg), "YAW=%.1f", angles.yaw);
//         k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
//         k_msleep(50);
//     }

//     /* -------- 5. 延时 -------- */
//     k_sleep(K_MSEC(LOOP_INTERVAL_MS));
//   }
// }


void main(void) {
    LOG_INF("System start...");

    int err;

    /* ---- 初始化 BLE，并注册 RX 回调 ---- */
    err = ble_nus_run(root_rx_cb);
    if (err) {
        LOG_ERR("BLE init failed: %d", err);
        return;
    }
    LOG_INF("BLE ready");

    /* ------------ 初始化 IMU ------------ */
    if (imu_init() != 0) {
        LOG_ERR("IMU init failed!");
        return;
    }
    LOG_INF("IMU OK");

    /* ------------ 初始化 ADC ------------ */
    if (adc_demo_init() != 0) {
        LOG_ERR("ADC init failed!");
        return;
    }
    LOG_INF("ADC OK");

    /* ------------ 初始化 Servo ------------ */
    if (servo_init() != 0) {
        LOG_ERR("Servo init failed!");
        return;
    }
    LOG_INF("Servo OK and centered");

    /* ========= 在 main 里做 IMU 参考姿态 ========= */
    struct imu_angles imu_ref = {0};
    bool imu_ref_set = false;
    static int imu_sample_count = 0;   /* 统计成功读到的次数 */

    /* ------------ 主循环 ------------ */
    while (1) {

        struct imu_angles angles;      // 绝对角
        struct imu_angles rel;         // 相对角
        bool rel_valid = false;        // 这一轮是否有相对角可用

        /* -------- 1. 读取姿态角（绝对） -------- */
        err = imu_update(&angles);
        if (err != 0) {
            LOG_WRN("imu_update() not ready, err=%d", err);
        } else {
            imu_sample_count++;

            /* 第 1~4 次：只让滤波器稳定，不做任何姿势判断 */
            if (!imu_ref_set && imu_sample_count < 5) {
                LOG_INF("IMU warm-up sample %d: R=%.2f P=%.2f Y=%.2f",
                        imu_sample_count,
                        angles.roll, angles.pitch, angles.yaw);
            }
            /* 第 5 次：锁定参考姿态 */
            else if (!imu_ref_set && imu_sample_count == 5) {
                imu_ref = angles;
                imu_ref_set = true;

                LOG_INF("IMU reference locked (5th sample): "
                        "R0=%.2f, P0=%.2f, Y0=%.2f",
                        imu_ref.roll, imu_ref.pitch, imu_ref.yaw);
            }

            /* 参考姿态已经锁定 → 计算相对角，做姿势判断 */
            if (imu_ref_set) {
                rel.roll  = angles.roll  - imu_ref.roll;
                rel.pitch = angles.pitch - imu_ref.pitch;
                rel.yaw   = angles.yaw   - imu_ref.yaw;
                rel_valid = true;

                printk("\n");
                printk("IMU ANGLES (relative to reference):\n");
                printk("  ROLL = %.2f deg\n",  rel.roll);
                printk("  PITCH= %.2f deg\n",  rel.pitch);
                printk("  YAW  = %.2f deg\n",  rel.yaw);

                /* -------- 2. 坏姿势检测（用相对角度） -------- */
                bool bad_posture =
                    (fabsf(rel.roll)  > POSTURE_ANGLE_THRESHOLD) ||
                    (fabsf(rel.pitch) > POSTURE_ANGLE_THRESHOLD);

                if (bad_posture) {
                    LOG_INF("Bad posture detected → servo tighten");
                    servo_apply_command('1'); // tighten
                    k_sleep(K_MSEC(200));
                } else {
                    LOG_INF("Posture OK (R=%.2f, P=%.2f)",
                            rel.roll, rel.pitch);
                }
            }
        }

        /* -------- 3. 读取压力传感器 -------- */
        int pressure = adc_demo_read();
        if (pressure < 0) {
            LOG_ERR("ADC read error: %d", pressure);
        } else {
            LOG_INF("Pressure = %d", pressure);
        }

        /* -------- 4. 压力过低 → 放松（发“2”命令） -------- */
        if (pressure < PRESSURE_LOWER_LIMIT) {
            LOG_INF("Pressure low → servo loosen");
            servo_apply_command('2'); // loosen
            k_sleep(K_MSEC(200));
        }

        printk("\n"); // 打印一行空行
        LOG_INF(" ");

        /* --------------------------------------------------
         *  sending data to phone via ble
         * -------------------------------------------------- */
        if (ble_nus_ready() && imu_ref_set && rel_valid) {

            char msg[32];

            /* Send pressure */
            snprintf(msg, sizeof(msg), "Pressure=%d", pressure);
            k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
            k_msleep(50);   // cool-down

            /* Send roll (relative) */
            snprintf(msg, sizeof(msg), "Roll=%.1f", rel.roll);
            k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
            k_msleep(50);

            /* Send pitch (relative) */
            snprintf(msg, sizeof(msg), "Pitch=%.1f", rel.pitch);
            k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
            k_msleep(50);

            /* Send yaw (relative) */
            snprintf(msg, sizeof(msg), "YAW=%.1f", rel.yaw);
            k_msgq_put(&ble_tx_queue, msg, K_NO_WAIT);
            k_msleep(50);
        }

        /* -------- 5. 延时 -------- */
        k_sleep(K_MSEC(LOOP_INTERVAL_MS));
    }
}



