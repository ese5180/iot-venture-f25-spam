/* mds.h - Memfault Diagnostic Service Module Header */

#ifndef MDS_H
#define MDS_H

/**
 * @file mds.h
 * @brief Memfault Diagnostic Service (MDS) Bluetooth module
 * 
 * This module handles all Bluetooth Low Energy functionality including:
 * - Connection management
 * - Security and pairing
 * - Advertising
 * - MDS service callbacks
 */

/**
 * @brief Initialize the MDS module and register Bluetooth callbacks.
 * 
 * This function:
 * - Initializes advertising work queue
 * - Registers MDS service callbacks
 * - Registers authentication callbacks
 * - Registers pairing information callbacks
 * 
 * Must be called before bt_enable() and before mds_start_advertising().
 * 
 * @return 0 on success, or negative error code on failure.
 */
int mds_module_init(void);

/**
 * @brief Start Bluetooth LE advertising.
 * 
 * Submits advertising work to the system workqueue. Advertising will
 * include the MDS service UUID and device name.
 * 
 * Can be called multiple times (e.g., after disconnect).
 */
void mds_start_advertising(void);

#endif /* MDS_H */
