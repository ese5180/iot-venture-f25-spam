// /* main.c - Application Entry Point */
// /*
//  * Copyright (c) 2022 Nordic Semiconductor ASA
//  *
//  * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
//  */

// #include <zephyr/kernel.h>
// #include <zephyr/bluetooth/bluetooth.h>
// #include <zephyr/bluetooth/services/bas.h>
// #include <zephyr/settings/settings.h>
// #include <dk_buttons_and_leds.h>
// // #include <memfault/metrics/metrics.h>
// // #include <memfault/core/trace_event.h>

// #include "mds.h"  // Our MDS Bluetooth module

// #define RUN_STATUS_LED DK_LED1
// #define RUN_LED_BLINK_INTERVAL 1000

// /* --- Battery Service Logic --- */

// static void bas_notify(void)
// {
//     int err;
//     uint8_t battery_level = bt_bas_get_battery_level();

//     __ASSERT_NO_MSG(battery_level > 0);

//     battery_level--;

//     if (battery_level == 0) {
//         battery_level = 100U;
//     }

//     err = MEMFAULT_METRIC_SET_UNSIGNED(battery_soc_pct, battery_level);
//     if (err) {
//         printk("Failed to set battery_soc_pct memfault metrics (err %d)\n", err);
//     }

//     bt_bas_set_battery_level(battery_level);
// }

// static void bas_work_handler(struct k_work *work)
// {
//     bas_notify();
//     k_work_reschedule((struct k_work_delayable *)work, K_SECONDS(1));
// }

// static K_WORK_DELAYABLE_DEFINE(bas_work, bas_work_handler);

// /* --- Button Handler with Memfault Metrics --- */

// static void button_handler(uint32_t button_state, uint32_t has_changed)
// {
//     static bool time_measure_start;

//     int err;
//     uint32_t buttons = button_state & has_changed;

//     if (buttons & DK_BTN1_MSK) {
//         time_measure_start = !time_measure_start;

//         if (time_measure_start) {
//             err = MEMFAULT_METRIC_TIMER_START(button_elapsed_time_ms);
//             if (err) {
//                 printk("Failed to start memfault metrics timer: %d\n", err);
//             }
//         } else {
//             err = MEMFAULT_METRIC_TIMER_STOP(button_elapsed_time_ms);
//             if (err) {
//                 printk("Failed to stop memfault metrics: %d\n", err);
//             }

//             /* Trigger collection of heartbeat data. */
//             memfault_metrics_heartbeat_debug_trigger();
//         }
//     }

//     if (has_changed & DK_BTN2_MSK) {
//         bool button_state_val = (buttons & DK_BTN2_MSK) ? 1 : 0;

//         MEMFAULT_TRACE_EVENT_WITH_LOG(button_state_changed, "Button state: %u",
//                           button_state_val);

//         printk("button_state_changed event has been tracked, button state: %u\n",
//                button_state_val);
//     }

//     if (buttons & DK_BTN3_MSK) {
//         err = MEMFAULT_METRIC_ADD(button_press_count, 1);
//         if (err) {
//             printk("Failed to increase button_press_count metric: %d\n", err);
//         } else {
//             printk("button_press_count metric increased\n");
//         }
//     }

//     if (buttons & DK_BTN4_MSK) {
//         volatile uint32_t i;

//         printk("Division by zero will now be triggered\n");
// #pragma GCC diagnostic push
// #pragma GCC diagnostic ignored "-Wdiv-by-zero"
//         i = 1 / 0;
// #pragma GCC diagnostic pop
//         ARG_UNUSED(i);
//     }
// }

// /* --- Main Application --- */

// int main(void)
// {
//     uint32_t blink_status = 0;
//     int err;

//     printk("Starting Bluetooth Memfault sample\n");

//     err = dk_leds_init();
//     if (err) {
//         printk("LEDs init failed (err %d)\n", err);
//         return 0;
//     }

//     err = dk_buttons_init(button_handler);
//     if (err) {
//         printk("Failed to initialize buttons (err %d)\n", err);
//         return 0;
//     }

//     /* Initialize MDS module (replaces bt_mds_cb_register + auth callbacks) */
//     err = mds_module_init();
//     if (err) {
//         printk("MDS module init failed (err %d)\n", err);
//         return 0;
//     }

//     err = bt_enable(NULL);
//     if (err) {
//         printk("Bluetooth init failed (err %d)\n", err);
//         return 0;
//     }

//     printk("Bluetooth initialized\n");

//     if (IS_ENABLED(CONFIG_SETTINGS)) {
//         err = settings_load();
//         if (err) {
//             printk("Failed to load settings (err %d)\n", err);
//             return 0;
//         }
//     }

//     /* Start advertising (replaces k_work_init + advertising_start) */
//     mds_start_advertising();

//     /* Schedule battery service updates */
//     k_work_schedule(&bas_work, K_SECONDS(1));

//     /* Main loop - blink LED */
//     for (;;) {
//         dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
//         k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));
//     }
// }
