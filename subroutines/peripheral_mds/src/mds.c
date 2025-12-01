/* mds.c - Memfault Diagnostic Service Module Implementation */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <bluetooth/services/mds.h>
#include <dk_buttons_and_leds.h>

#include "mds.h"

#define DEVICE_NAME     CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)
#define CON_STATUS_LED  DK_LED2

static struct bt_conn *mds_conn;
static struct k_work adv_work;

/* Forward declaration */
void mds_start_advertising(void);

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_MDS_VAL),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

/* --- BLE Connection & Security Callbacks --- */

static void security_changed(struct bt_conn *conn, bt_security_t level,
                 enum bt_security_err err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (!err) {
        printk("Security changed: %s level %u\n", addr, level);
    } else {
        printk("Security failed: %s level %u err %d %s\n", addr, level, err,
               bt_security_err_to_str(err));
    }

    if (level >= BT_SECURITY_L2) {
        if (!mds_conn) {
            mds_conn = conn;
        }
    }
}

static void adv_work_handler(struct k_work *work)
{
    int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));

    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return;
    }

    printk("Advertising successfully started\n");
}

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    if (conn_err) {
        printk("Connection failed, err 0x%02x %s\n", conn_err, bt_hci_err_to_str(conn_err));
        return;
    }

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    printk("Connected %s\n", addr);

    dk_set_led_on(CON_STATUS_LED);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    printk("Disconnected, reason 0x%02x %s\n", reason, bt_hci_err_to_str(reason));

    dk_set_led_off(CON_STATUS_LED);

    if (conn == mds_conn) {
        mds_conn = NULL;
    }
}

static void recycled_cb(void)
{
    printk("Connection object available from previous conn. Disconnect is complete!\n");
    mds_start_advertising();
}

/* Register connection callbacks */
BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected        = connected,
    .disconnected     = disconnected,
    .security_changed = security_changed,
    .recycled         = recycled_cb,
};

/* --- Pairing & Authentication Callbacks --- */

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Pairing completed: %s, bonded: %d\n", addr, bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Pairing failed conn: %s, reason %d %s\n", addr, reason,
           bt_security_err_to_str(reason));
}

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
    .pairing_complete = pairing_complete,
    .pairing_failed = pairing_failed
};

static void auth_cancel(struct bt_conn *conn)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
    .cancel = auth_cancel,
};

/* --- MDS Service Callbacks --- */

static bool mds_access_enable(struct bt_conn *conn)
{
    if (mds_conn && (conn == mds_conn)) {
        return true;
    }

    return false;
}

static const struct bt_mds_cb mds_cb = {
    .access_enable = mds_access_enable,
};

/* --- Public API Functions --- */

void mds_start_advertising(void)
{
    k_work_submit(&adv_work);
}

int mds_module_init(void)
{
    int err;

    /* Initialize the advertising work item */
    k_work_init(&adv_work, adv_work_handler);

    /* Register MDS callbacks */
    err = bt_mds_cb_register(&mds_cb);
    if (err) {
        printk("Memfault Diagnostic service callback registration failed (err %d)\n", err);
        return err;
    }

    /* Register Auth callbacks */
    err = bt_conn_auth_cb_register(&conn_auth_callbacks);
    if (err) {
        printk("Failed to register authorization callbacks (err %d)\n", err);
        return err;
    }

    err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
    if (err) {
        printk("Failed to register authorization info callbacks (err %d)\n", err);
        return err;
    }

    return 0;
}
