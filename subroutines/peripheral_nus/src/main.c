// /*
//  * Copyright (c) 2024 Croxel, Inc.
//  *
//  * SPDX-License-Identifier: Apache-2.0
//  */

// #include <zephyr/kernel.h>
// #include <zephyr/bluetooth/bluetooth.h>
// #include <zephyr/bluetooth/services/nus.h>

// #define DEVICE_NAME		"Xiaopeng"
// #define DEVICE_NAME_LEN		(sizeof(DEVICE_NAME) - 1)

// static const struct bt_data ad[] = {
// 	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
// 	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
// };

// static const struct bt_data sd[] = {
// 	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
// };

// static void notif_enabled(bool enabled, void *ctx)
// {
// 	ARG_UNUSED(ctx);

// 	printk("%s() - %s\n", __func__, (enabled ? "Enabled" : "Disabled"));
// }

// static void received(struct bt_conn *conn, const void *data, uint16_t len, void *ctx)
// {
// 	char message[CONFIG_BT_L2CAP_TX_MTU + 1] = "";

// 	ARG_UNUSED(conn);
// 	ARG_UNUSED(ctx);

// 	memcpy(message, data, MIN(sizeof(message) - 1, len));
// 	printk("%s() - Len: %d, Message: %s\n", __func__, len, message);
// }

// struct bt_nus_cb nus_listener = {
// 	.notif_enabled = notif_enabled,
// 	.received = received,
// };

// int main(void)
// {
// 	int err;

// 	printk("Sample - Bluetooth Peripheral NUS\n");

// 	err = bt_nus_cb_register(&nus_listener, NULL);
// 	if (err) {
// 		printk("Failed to register NUS callback: %d\n", err);
// 		return err;
// 	}

// 	err = bt_enable(NULL);
// 	if (err) {
// 		printk("Failed to enable bluetooth: %d\n", err);
// 		return err;
// 	}
//khjgjhghv
// 	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
// 	if (err) {
// 		printk("Failed to start advertising: %d\n", err);
// 		return err;
// 	}

// 	printk("Initialization complete\n");

// 	while (true) {
// 		const char *hello_world = "Hello World!\n";

// 		k_sleep(K_SECONDS(3));

// 		err = bt_nus_send(NULL, hello_world, strlen(hello_world));
// 		printk("Data send - Result: %d\n", err);

// 		if (err < 0 && (err != -EAGAIN) && (err != -ENOTCONN)) {
// 			return err;
// 		}
// 	}

// 	return 0;
// }

/*
 * Combined Bluetooth NUS + BME280 Example
 * Author: Xiaopeng
 * Platform: nRF7002DK (nRF5340 Application Core)
 * SDK: nRF Connect SDK v3.0.2 (Zephyr 3.x)
 */

// #include <zephyr/kernel.h>
// #include <zephyr/device.h>
// #include <zephyr/drivers/sensor.h>
// #include <zephyr/bluetooth/bluetooth.h>
// #include <zephyr/bluetooth/services/nus.h>
// #include <string.h>

// #define DEVICE_NAME      "Xiaopeng"
// #define DEVICE_NAME_LEN  (sizeof(DEVICE_NAME) - 1)
// #define SEND_INTERVAL_MS 2000   /* 2 秒发送一次 */

// /* ====== Bluetooth Advertisement (global, cannot be in main) ====== */
// static const struct bt_data ad[] = {
// 	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
// 	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
// };

// static const struct bt_data sd[] = {
// 	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
// };

// /* ====== Global handles ====== */
// static const struct device *bme280_dev;
// static struct bt_conn *current_conn = NULL;

// /* ====== BLE connection callbacks ====== */
// static void connected(struct bt_conn *conn, uint8_t err)
// {
// 	if (!err) {
// 		current_conn = bt_conn_ref(conn);
// 	}
// }

// static void disconnected(struct bt_conn *conn, uint8_t reason)
// {
// 	if (current_conn) {
// 		bt_conn_unref(current_conn);
// 		current_conn = NULL;
// 	}
// }

// BT_CONN_CB_DEFINE(conn_callbacks) = {
// 	.connected = connected,
// 	.disconnected = disconnected,
// };

// /* ====== Optional NUS receive callback ====== */
// static void received(struct bt_conn *conn, const void *data, uint16_t len, void *ctx)
// {
// 	ARG_UNUSED(conn);
// 	ARG_UNUSED(ctx);
// 	ARG_UNUSED(data);
// 	ARG_UNUSED(len);
// 	/* 如果你需要处理手机发来的数据，可以在这里加逻辑 */
// }

// static struct bt_nus_cb nus_callbacks = {
// 	.received = received,
// };

// /* ====== Main ====== */
// int main(void)
// {
// 	int err;
// 	struct sensor_value temp, press, hum;

// 	/* 1️⃣ 启动 Bluetooth  */
// 	err = bt_enable(NULL);
// 	if (err) {
// 		return err;
// 	}

// 	/* 2️⃣ 启动 Advertising  */
// 	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
// 	if (err) {
// 		return err;
// 	}

// 	/* 3️⃣ 注册 NUS 回调（新 API） */
// 	bt_nus_cb_register(&nus_callbacks, NULL);

// 	/* 4️⃣ 初始化 BME280  */
// 	bme280_dev = DEVICE_DT_GET_ANY(bosch_bme280);
// 	if (bme280_dev == NULL || !device_is_ready(bme280_dev)) {
// 		return -1;
// 	}

// 	/* 5️⃣ 循环采集并发送  */
// 	while (1) {
// 		if (sensor_sample_fetch(bme280_dev) == 0) {
// 			sensor_channel_get(bme280_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
// 			sensor_channel_get(bme280_dev, SENSOR_CHAN_PRESS, &press);
// 			sensor_channel_get(bme280_dev, SENSOR_CHAN_HUMIDITY, &hum);

// 			char msg[64];
// 			snprintf(msg, sizeof(msg), "%.2f,%.2f,%.2f\r\n",
// 				sensor_value_to_double(&temp),
// 				sensor_value_to_double(&press) / 1000.0,
// 				sensor_value_to_double(&hum));

// 			if (current_conn) {
// 				bt_nus_send(NULL, msg, strlen(msg));
// 			}
// 		}
// 		k_msleep(SEND_INTERVAL_MS);
// 	}
// }
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);
const struct device *dev = DEVICE_DT_GET_ANY(bosch_bme280);



#define DEVICE_NAME		"Xiaopeng"
#define DEVICE_NAME_LEN		(sizeof(DEVICE_NAME) - 1)

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
};

static void notif_enabled(bool enabled, void *ctx)
{
	ARG_UNUSED(ctx);

	printk("%s() - %s\n", __func__, (enabled ? "Enabled" : "Disabled"));
}

static void received(struct bt_conn *conn, const void *data, uint16_t len, void *ctx)
{
	char message[CONFIG_BT_L2CAP_TX_MTU + 1] = "";

	ARG_UNUSED(conn);
	ARG_UNUSED(ctx);

	memcpy(message, data, MIN(sizeof(message) - 1, len));
	printk("%s() - Len: %d, Message: %s\n", __func__, len, message);
}

struct bt_nus_cb nus_listener = {
	.notif_enabled = notif_enabled,
	.received = received,
};

int main(void)
{
	int err;

	printk("Sample - Bluetooth Peripheral NUS\n");

	err = bt_nus_cb_register(&nus_listener, NULL);
	if (err) {
		printk("Failed to register NUS callback: %d\n", err);
		return err;
	}

	err = bt_enable(NULL);
	if (err) {
		printk("Failed to enable bluetooth: %d\n", err);
		return err;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Failed to start advertising: %d\n", err);
		return err;
	}

	
    if (dev == NULL) {
        LOG_ERR("No BME280 device found");
        return -1;
    }

    if (!device_is_ready(dev)) {
        LOG_ERR("BME280 device not ready");
        return -1;
    }

    LOG_INF("BME280 initialized successfully!");


	printk("Initialization complete\n");

	
struct sensor_value temp;
while (true) {
    /* 读取 BME280 温度 */
    int ret = sensor_sample_fetch(dev);
    if (ret) {
        printk("BME280 fetch failed: %d\n", ret);
        k_sleep(K_SECONDS(2));
        continue;
    }

    sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);

    char data_str[32];
    snprintf(data_str, sizeof(data_str), "T: %.2f °C\n",
             sensor_value_to_double(&temp));

    err = bt_nus_send(NULL, data_str, strlen(data_str));
    printk("BLE send: %s (err: %d)\n", data_str, err);

    if (err == -ENOMEM) {
        printk("No buffer, skipping this send.\n");
        k_msleep(100);
        continue;
    } else if (err < 0 && err != -EAGAIN && err != -ENOTCONN) {
        printk("bt_nus_send failed with err %d\n", err);
    }

    /* 每5秒发一次 */
    k_sleep(K_SECONDS(5));
}

return 0;
}
