/* root/src/main.c - BLE NUS → Servo bridge */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ble_nus.h"
#include "servo.h"

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
    int err;

    LOG_INF("Root app starting: BLE NUS -> servo");

    /* Initialize servo hardware (PWM, center position, etc.) */
    err = servo_init();
    if (err) {
        LOG_ERR("servo_init failed: %d", err);
        return 0;
    }

    /* Initialize BLE + NUS using the ble_nus subroutine */
    err = ble_nus_run(root_rx_cb);
    if (err) {
        LOG_ERR("ble_nus_run failed: %d", err);
        return 0;
    }

    LOG_INF("System ready. Use phone app (NUS) to send '1'/'2' to move servo.");

    while (1) {
        k_sleep(K_FOREVER);
    }
}
