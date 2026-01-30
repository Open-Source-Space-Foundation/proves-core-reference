/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal driver skeleton for APMemory APS1604M QSPI PSRAM.
 * TO DO::: Fill in actual QSPI/SPI FOR RP2350 THIS IS A STUB FOR COMPILING!!!!!!!!!!!!!!!!!!!!!!!
 */

#define DT_DRV_COMPAT aps_aps1604m

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(aps1604m, CONFIG_PSRAM_LOG_LEVEL);

struct aps1604m_config {
    size_t size_bytes;
    uint32_t spi_max_frequency;
};

struct aps1604m_data {
    /* Add runtime state (e.g. lock, cache) when you implement read/write */
};

static int aps1604m_init(const struct device* dev) {
    const struct aps1604m_config* cfg = dev->config;

    ARG_UNUSED(dev);

    LOG_INF("APS1604M PSRAM: size %zu bytes, max SPI freq %u Hz", cfg->size_bytes, cfg->spi_max_frequency);

    /* TODO: Initialize QSPI/SPI for this device.
     * On RP2350, QMI is used for internal flash; using a second CS for
     * external PSRAM may require SoC-specific or pico-SDK support.
     */

    return 0;
}

/* Optional: expose size for other code until you add a full API */
size_t aps1604m_get_size(const struct device* dev) {
    const struct aps1604m_config* cfg = dev->config;
    return cfg->size_bytes;
}

#define APS1604M_INIT(inst)                                                                                       \
    static struct aps1604m_data aps1604m_data_##inst;                                                             \
                                                                                                                  \
    static const struct aps1604m_config aps1604m_config_##inst = {                                                \
        .size_bytes = DT_INST_REG_SIZE(inst),                                                                     \
        .spi_max_frequency = DT_INST_PROP_OR(inst, spi_max_frequency, 84000000),                                  \
    };                                                                                                            \
                                                                                                                  \
    DEVICE_DT_INST_DEFINE(inst, aps1604m_init, NULL, &aps1604m_data_##inst, &aps1604m_config_##inst, POST_KERNEL, \
                          CONFIG_PSRAM_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(APS1604M_INIT)
