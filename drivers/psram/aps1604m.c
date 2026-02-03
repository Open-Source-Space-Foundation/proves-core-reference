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

/* APS1604M instruction set */
#define APS1604M_CMD_READ 0x03U       // Read Memory Code
#define APS1604M_CMD_FAST_READ 0x0BU  // Fast Read Memory Code
#define ASP1604M_CMD_READ_QUAD 0xEBU  // Quad Read Memory Code

#define APS1604M_CMD_WRITE 0x02U       // Write Memory Code
#define APS1604M_CMD_WRITE_QUAD 0x38U  // Quad Write Memory Code

#define APS1604M_CMD_WRAPPED_READ 0x8BU   // Wrapped Read Memory Code
#define APS1604M_CMD_WRAPPED_WRITE 0x82U  // Wrapped Write Memory Code

#define APS1604M_CMD_REGISTER_READ 0xB5U   // Register Read Memory Code
#define APS1604M_CMD_REGISTER_WRITE 0xB1U  // Register Write Memory Code

#define APS1604M_CMD_ENTER_QUAD_MODE 0x35U  // Enter Quad Mode
#define APS1604M_CMD_EXIT_QUAD_MODE 0xF5U   // Exit Quad Mode

#define APS1604M_CMD_RESET_ENABLE 0x66U  // Reset Enable
#define APS1604M_CMD_RESET 0x99U         // Reset

#define APS1604M_CMD_BURST_LENGTH_TOGGLE 0xC0U  // Burst Length Toggle
#define APS1604M_CMD_READ_ID 0x9FU              // Read ID

struct aps1604m_config {
    size_t size_bytes;
    uint32_t spi_max_frequency;
    bool readonly;
};

struct aps1604m_data {
    struct k_mutex lock;
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
