/* root/src/main.c - BLE NUS → Servo bridge */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "madgwick.h"
#include "imu.h" 


LOG_MODULE_REGISTER(root_main, LOG_LEVEL_INF);

/* Callback for data coming from phone over NUS */
static void root_rx_cb(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        char cmd = (char)data[i];

        LOG_INF("RX cmd '%c' (0x%02x)",
                (cmd >= 32 && cmd <= 126) ? cmd : '.', (unsigned char)cmd);

        /* Use servo subroutine API:
         *  '1' => +90 deg
         *  '2' => -90 deg
         *  '0', 'c', 'C' => center (based on your servo_apply_command())
         */
        servo_apply_command(cmd);
    }
}

int main(void)
{
    LOG_INF("System Start");

    /* ===========================================
     *          1. 初始化 IMU（含自动校准）
     * =========================================== */
    int err = imu_init();
    if (err != 0) {
        LOG_ERR("IMU init failed! err=%d", err);
        return 0;
    }

    LOG_INF("IMU initialized successfully!");
    LOG_INF("Calibration finished. Entering measurement mode...");

    /* ===========================================
     *          2. 主循环：每 500ms 测一次
     * =========================================== */
    struct imu_angles angles;
    const float threshold_deg = 10.0f;

    while (1) {

    imu_update(&angles);

    LOG_INF("ROLL=%.2f  PITCH=%.2f  YAW=%.2f",
            angles.roll, angles.pitch, angles.yaw);

    // angles.xxx 已经是“偏移角度”
    if (fabsf(angles.roll)  > threshold_deg ||
        fabsf(angles.pitch) > threshold_deg ||
        fabsf(angles.yaw)   > threshold_deg) {

        LOG_ERR("⚠ Bad posture detected!");
    }

    k_msleep(500);
}
}