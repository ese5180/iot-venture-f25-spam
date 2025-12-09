/* ble_nus.c - Minimal Nordic UART Service helper for nRF7002DK (CPUAPP) */

#include "ble_nus.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/services/nus.h>
#include <bluetooth/services/mds.h>

/* Version-portable advertising parameters for NUS */
#if defined(BT_LE_ADV_CONN)
/* Older Zephyr / NCS (your local SDK) */
#define NUS_ADV_PARAMS BT_LE_ADV_CONN

#elif defined(BT_LE_ADV_CONN_FAST_1)
/* Zephyr 4.3.x in the GitHub Actions container */
#define NUS_ADV_PARAMS BT_LE_ADV_CONN_FAST_1

#else
/* Fallback: explicit param using options that *do* exist */
#define NUS_ADV_PARAMS                                                         \
  BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN, BT_GAP_ADV_FAST_INT_MIN_2,               \
                  BT_GAP_ADV_FAST_INT_MAX_2, NULL)
#endif

LOG_MODULE_REGISTER(ble_nus, LOG_LEVEL_INF);

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static bool ready = false;

static void notif_enabled(bool enabled, void *ctx) {
  ready = enabled;
  LOG_INF("Notifications %s", enabled ? "enabled" : "disabled");
}

bool ble_nus_ready(void) { return ready; }

/* Advertising payload:
 *  - General discoverable, no BR/EDR
 *  - 128-bit UUID for NUS
 */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
    
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_MDS_VAL),
};

/* Application-level RX handler that we call from the NUS callback */
static ble_nus_rx_handler_t app_rx_handler;

/* Connection callbacks just for logging */
static void connected(struct bt_conn *conn, uint8_t err) {
  if (err) {
    LOG_ERR("Connection failed (err %u)", err);
  } else {
    LOG_INF("BLE connected");
  }
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
  LOG_INF("BLE disconnected (reason 0x%02x)", reason);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

/* NUS RX callback
 * NOTE: signature must match struct bt_nus_cb in this NCS version:
 *   void (*received)(struct bt_conn *, const void *, uint16_t, void *)
 */
static void nus_received(struct bt_conn *conn, const void *data, uint16_t len,
                         void *user_data) {
  const uint8_t *bytes = data;

  LOG_INF("NUS received %u bytes", len);

  if (app_rx_handler) {
    app_rx_handler(bytes, len);
  }
}

/* Register our receive callback with the NUS service.
 * In this NCS/Zephyr version the callbacks are registered via
 * bt_nus_cb_register(), NOT bt_nus_init().
 */
static struct bt_nus_cb nus_cb = {
    .received = nus_received,
    .notif_enabled = notif_enabled,
};

int ble_nus_run(ble_nus_rx_handler_t handler) {
  int err;

  LOG_INF("Initializing BLE NUS");

  app_rx_handler = handler;

  /* First bring up the Bluetooth stack */
  err = bt_enable(NULL);
  if (err) {
    LOG_ERR("bt_enable failed (err %d)", err);
    return err;
  }

  LOG_INF("Bluetooth enabled");

  /* Load persisted identities/CCC settings so the controller gets an ID addr */
  if (IS_ENABLED(CONFIG_SETTINGS)) {
    err = settings_load();
    if (err) {
      LOG_ERR("settings_load failed (err %d)", err);
      return err;
    }
  }

  /* Wire our nus_received() into the NUS service */
  err = bt_nus_cb_register(&nus_cb, NULL);
  if (err) {
    LOG_ERR("bt_nus_cb_register failed (err %d)", err);
    return err;
  }

  LOG_INF("NUS callbacks registered");

  /* Start advertising */
  err = bt_le_adv_start(NUS_ADV_PARAMS, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
  if (err) {
    LOG_ERR("Advertising failed to start (err %d)", err);
    return err;
  }

  LOG_INF("Advertising as \"%s\" with NUS", DEVICE_NAME);

  return 0;
}
