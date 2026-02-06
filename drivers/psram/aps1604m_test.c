/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * Basic test for APS1604M PSRAM: read ID, write/read a byte in QSPI mode.
 * Call aps1604m_test(dev) from main; do not define main() here.
 */

#include "aps1604m.h"

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(aps1604m_test, CONFIG_PSRAM_LOG_LEVEL);

void aps1604m_test(const struct device* dev) {
    int err;
    uint8_t id[4];
    uint8_t value;

    printk("APS1604M test\n");

    if (dev == NULL) {
        LOG_ERR("PSRAM device is NULL");
        return;
    }
    printk("PSRAM device found\n");

    printk("Checking if PSRAM device is ready\n");
    if (!device_is_ready(dev)) {
        LOG_ERR("PSRAM device not ready");
        return;
    }
    printk("PSRAM device ready\n");

    err = aps1604m_read_id(dev, id, sizeof(id));
    if (err < 0) {
        LOG_ERR("Failed to read device ID: %d", err);
        return;
    }
    LOG_INF("Device ID: %02X %02X %02X %02X", id[0], id[1], id[2], id[3]);

    value = 0x55U;
    err = aps1604m_write_qspi(dev, 0, &value, sizeof(value));
    if (err < 0) {
        LOG_ERR("Failed to write: %d", err);
        return;
    }
    value = 0U;
    err = aps1604m_read_qspi(dev, 0, &value, sizeof(value));
    if (err < 0) {
        LOG_ERR("Failed to read: %d", err);
        return;
    }
    LOG_INF("Read back value: 0x%02X", value);
}
