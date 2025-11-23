#ifndef BLE_NUS_H_
#define BLE_NUS_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Application-level callback type for received NUS data */
typedef void (*ble_nus_rx_handler_t)(const uint8_t *data, size_t len);

/* Start BLE + NUS and begin advertising.
 *
 * 'handler' is called whenever a NUS RX packet is received from the phone.
 * Returns 0 on success, negative errno on failure.
 */
int ble_nus_run(ble_nus_rx_handler_t handler);

#ifdef __cplusplus
}
#endif

#endif /* BLE_NUS_H_ */
