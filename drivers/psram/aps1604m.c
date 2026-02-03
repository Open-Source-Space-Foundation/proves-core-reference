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

// status register bits
// Mode Register 0
// select with MODE_REGISTER_0 | MR0_OP0_Pos
// cam read or write it

#define MODE_REGISTER_0 0x00U
#define MR0_OP0_Pos 0  // Z
#define MR0_OP1_Pos 1  // Z
#define MR0_OP2_Pos 2  // Z
#define MR0_OP3_Pos 3  // DQ
#define MR0_OP4_Pos 4  // DQ
#define MR0_OP5_Pos 5  // DQ
#define MR0_OP6_Pos 6  // reserved
#define MR0_OP7_Pos 7  // reserved

static inline uint8_t build_mr0(uint8_t dq, uint8_t zou) {
    return ((dq & 0x07) << MR0_OP3_Pos) | ((zou & 0x07) << MR0_OP0_Pos);
}

struct aps1604m_config {
    size_t size_bytes;
    uint32_t spi_max_frequency;
    bool readonly;
};

struct aps1604m_data {
    struct k_mutex lock;
};

static int aps1604m_reset(const struct device* dev) {
    const struct aps1604m_config* cfg = dev->config;
    struct aps1604m_data* data = dev->data;
    uint8_t rst_en = APS1604M_CMD_RESET_ENABLE;
    uint8_t rst = APS1604M_CMD_RESET;
    int err;

    /* Datasheet: Reset must immediately follow Reset-Enable; no other command in between. */
    const struct spi_buf tx_rst_en = {.buf = &rst_en, .len = 1};
    const struct spi_buf_set tx_set_en = {.buffers = &tx_rst_en, .count = 1};

    const struct spi_buf tx_rst = {.buf = &rst, .len = 1};
    const struct spi_buf_set tx_set_rst = {.buffers = &tx_rst, .count = 1};

    k_mutex_lock(&data->lock, K_FOREVER);

    err = spi_write_dt(&cfg->spi, &tx_set_en);
    if (err < 0) {
        LOG_ERR("Reset-Enable failed %d", err);
        k_mutex_unlock(&data->lock);
        return err;
    }
    err = spi_write_dt(&cfg->spi, &tx_set_rst);
    if (err < 0) {
        LOG_ERR("Reset failed %d", err);
        k_mutex_unlock(&data->lock);
        return err;
    }

    k_mutex_unlock(&data->lock);
    // Wait for device to complete reset (datasheet: 150µs + reset; 100ms is safe).
    k_sleep(K_MSEC(100));
    return 0;
}

static int aps1604m_init(const struct device* dev) {
    const struct aps1604m_config* cfg = dev->config;
    struct aps1604m_data* data = dev->data;
    int err;

    // check if the SPI bus is ready
    if (!spi_is_ready_dt(&cfg->spi)) {
        LOG_ERR("SPI bus not ready");
        return -EINVAL;
    }

    err = aps1604m_reset(dev);
    if (err < 0) {
        LOG_ERR("Failed to reset device (err %d)", err);
        return err;
    }

    // read the device ID, to implement later
    // err = aps1604m_rdid(dev);
    // if (err < 0) {
    //     LOG_ERR("Failed to initialize device, RDID check failed (err %d)", err);
    //     return err;
    // }

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
