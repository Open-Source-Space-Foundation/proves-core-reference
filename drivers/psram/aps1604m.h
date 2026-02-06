/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * Public API for APMemory APS1604M QSPI PSRAM driver.
 */

#ifndef ZEPHYR_DRIVERS_PSRAM_APS1604M_H_
#define ZEPHYR_DRIVERS_PSRAM_APS1604M_H_

#include <stddef.h>
#include <sys/types.h>

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Read the device ID (RDID 0x9F) into a buffer.
 * @param dev PSRAM device (e.g. DEVICE_DT_GET(DT_NODELABEL(psram0)))
 * @param id  Buffer to receive ID bytes (typically 4 bytes)
 * @param len Number of bytes to read (max 4)
 * @return 0 on success, negative errno on failure
 */
int aps1604m_read_id(const struct device* dev, uint8_t* id, size_t len);

/**
 * Read from PSRAM in quad-SPI mode.
 * @param dev    PSRAM device
 * @param offset Byte offset in PSRAM
 * @param buf    Buffer to fill
 * @param len    Number of bytes to read
 * @return 0 on success, negative errno on failure
 */
int aps1604m_read_qspi(const struct device* dev, off_t offset, void* buf, size_t len);

/**
 * Write to PSRAM in quad-SPI mode.
 * @param dev    PSRAM device
 * @param offset Byte offset in PSRAM
 * @param buf    Data to write
 * @param len    Number of bytes to write
 * @return 0 on success, negative errno on failure
 */
int aps1604m_write_qspi(const struct device* dev, off_t offset, const void* buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_PSRAM_APS1604M_H_ */
