/* root/src/main.c - BLE NUS → Servo bridge */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ble_nus.h"
#include "servo.h"
#include "madgwick.h"
#include "imu.h"
#include "adc_demo.h"

LOG_MODULE_REGISTER(root_main, LOG_LEVEL_INF);

static void root_rx_cb(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        char cmd = (char)data[i];

        LOG_INF("RX cmd '%c' (0x%02x)",
                (cmd >= 32 && cmd <= 126) ? cmd : '.', (unsigned char)cmd);

        servo_apply_command(cmd);
    }
}

void main(void)
{
    LOG_INF("System start...");

    /* ========== 1. 初始化 IMU ========== */
    if (imu_init() != 0) {
        LOG_ERR("IMU init failed");
        return;
    }
    LOG_INF("IMU init OK");

    /* ========== 2. 初始化 ADC（压力传感器） ========== */
    if (adc_demo_init() != 0) {
        LOG_ERR("Pressure sensor init failed");
        return;
    }
    LOG_INF("Pressure sensor init OK");

    /* ========== 3. 主循环：读取 IMU + ADC ========== */
    while (1) {

        /* ---- a. 获取 IMU 角度 ---- */
        struct imu_angles angles;
        imu_update(&angles);

        printk("ROLL=%.2f  PITCH=%.2f  YAW=%.2f  ",
                angles.roll, angles.pitch, angles.yaw);

        /* ---- b. 获取 ADC 数值 ---- */
        int adc_raw = adc_demo_read();    // 返回值就是采样数

        if (adc_raw < 0) {
            LOG_ERR("ADC read failed (%d)", adc_raw);
        } else {
            LOG_INF("Pressure ADC raw = %d", adc_raw);
        }

        /* ---- c. Delay ---- */
        k_sleep(K_MSEC(1000));
    }
}
