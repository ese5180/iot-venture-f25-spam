#include "ble_nus.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

/* Simple application handler: print whatever comes from the phone */
static void app_rx_handler(const uint8_t *data, size_t len) {
  LOG_INF("APP RX: %u bytes", (unsigned int)len);

  /* Print as ASCII to the RTT/UART console */
  for (size_t i = 0; i < len; i++) {
    printk("%c", data[i]);
  }
  printk("\r\n");
}

int main(void) {
  int err = ble_nus_run(app_rx_handler);

  if (err) {
    LOG_ERR("ble_nus_run failed (err %d)", err);
    return err;
  }

  /* Nothing else to do; all work happens in callbacks */
  while (1) {
    k_sleep(K_SECONDS(1));
  }

  return 0;
}
