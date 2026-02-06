/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * APMemory APS1604M 16Mbit (2MB) QSPI PSRAM driver.
 *
 * Datasheet: power-ramp 150µs (board); driver adds 200µs before first SPI access.
 * Address A[20:0]; Enter Quad (0x35) in init for quad read/write.
 */

#define DT_DRV_COMPAT aps_aps1604m_qmi

#include "aps1604m.h"

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(aps1604m, CONFIG_PSRAM_LOG_LEVEL);

/* APS1604M instruction set */
#define APS1604M_CMD_READ 0x03U       // Read Memory Code
#define APS1604M_CMD_FAST_READ 0x0BU  // Fast Read Memory Code
#define APS1604M_CMD_READ_QUAD 0xEBU  // Quad Read Memory Code (

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
    const struct device* parent;
    struct gpio_dt_spec cs;
    size_t size_bytes;
    uint32_t spi_max_frequency;
    const struct pinctrl_dev_config* pcfg;
};

struct aps1604m_data {
    struct k_mutex lock;
};

static int aps1604m_enter_quad_mode_unused(const struct device* dev) {
    const struct aps1604m_config* cfg = dev->config;
    struct aps1604m_data* data = dev->data;
    uint8_t cmd = APS1604M_CMD_ENTER_QUAD_MODE;
    const struct spi_buf tx_buf = {.buf = &cmd, .len = 1};
    const struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
    int err;

    k_mutex_lock(&data->lock, K_FOREVER);
    err = spi_write_dt(&cfg->bus.spi, &tx);
    k_mutex_unlock(&data->lock);
    if (err < 0) {
        LOG_ERR("Enter Quad Mode failed %d", err);
        return err;
    }
    return 0;
}

static int aps1604m_init(const struct device* dev) {
    const struct aps1604m_config* cfg = dev->config;
    int err;

    /* Apply pinctrl only for SPI path; QMI uses dedicated QSPI pins owned by the controller. */
    if (cfg->pcfg != NULL && !cfg->is_qmi) {
        err = pinctrl_apply_state(cfg->pcfg, PINCTRL_STATE_DEFAULT);
        if (err < 0) {
            LOG_ERR("pinctrl apply failed %d", err);
            return err;
        }
    }

    /* Datasheet: from power ramp to end of 150µs, CLK low, CE# high, SI/SO/SIO low. */
    k_busy_wait(200);

    if (cfg->is_qmi) {
        /* Under QMI flash controller: transfers not yet implemented; init minimal. */
        LOG_WRN("APS1604M under QMI: transfer API not implemented");
        return 0;
    }

    if (!spi_is_ready_dt(&cfg->bus.spi)) {
        LOG_ERR("SPI bus not ready");
        return -EINVAL;
    }

    err = aps1604m_reset(dev);
    if (err < 0) {
        LOG_ERR("Failed to reset device (err %d)", err);
        return err;
    }

    err = aps1604m_rdid(dev);
    if (err < 0) {
        LOG_ERR("Failed to initialize device, RDID check failed (err %d)", err);
        return err;
    }

    /* Device powers up in SPI mode; send Enter Quad (0x35) so quad read/write work. */
    err = aps1604m_enter_quad_mode(dev);
    if (err < 0) {
        LOG_ERR("Enter Quad Mode failed (err %d)", err);
        return err;
    }

    return 0;
}

/*
 * APS1604M command sequence (datasheet): opcode (1 byte) + 3-byte address (A[20:0], MSB first) + data.
 * 2MB = 21-bit address; READ 0x03 / WRITE 0x02. Quad commands (0xEB/0x38) may have dummy cycles per datasheet.
 */

__attribute__((unused)) static int aps1604m_regular_read(const struct device* dev,
                                                         off_t offset,
                                                         void* buf,
                                                         size_t len) {
    const struct aps1604m_config* cfg = dev->config;
    struct aps1604m_data* data = dev->data;
    uint8_t cmd[4];
    int err;

    if (offset + len > cfg->size_bytes || len == 0) {
        return -EINVAL;
    }

    cmd[0] = APS1604M_CMD_READ;
    cmd[1] = (offset >> 16) & 0xFF;
    cmd[2] = (offset >> 8) & 0xFF;
    cmd[3] = offset & 0xFF;

    const struct spi_buf tx_buf = {.buf = cmd, .len = sizeof(cmd)};
    const struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
    const struct spi_buf rx_bufs[2] = {
        {.buf = NULL, .len = sizeof(cmd)},
        {.buf = buf, .len = len},
    };
    const struct spi_buf_set rx = {.buffers = rx_bufs, .count = ARRAY_SIZE(rx_bufs)};

    k_mutex_lock(&data->lock, K_FOREVER);
    err = spi_transceive_dt(&cfg->bus.spi, &tx, &rx);
    k_mutex_unlock(&data->lock);

    if (err < 0) {
        LOG_ERR("read failed %d", err);
    }
    return err;
}

__attribute__((unused)) static int aps1604m_regular_write(const struct device* dev,
                                                          off_t offset,
                                                          const void* buf,
                                                          size_t len) {
    const struct aps1604m_config* cfg = dev->config;
    struct aps1604m_data* data = dev->data;
    uint8_t cmd[4];
    int err;

    if (offset + len > cfg->size_bytes || len == 0) {
        return -EINVAL;
    }

    cmd[0] = APS1604M_CMD_WRITE;
    cmd[1] = (offset >> 16) & 0xFF;
    cmd[2] = (offset >> 8) & 0xFF;
    cmd[3] = offset & 0xFF;

    const struct spi_buf tx_bufs[2] = {
        {.buf = cmd, .len = sizeof(cmd)},
        {.buf = (void*)buf, .len = len},
    };
    const struct spi_buf_set tx = {.buffers = tx_bufs, .count = ARRAY_SIZE(tx_bufs)};

    k_mutex_lock(&data->lock, K_FOREVER);
    err = spi_write_dt(&cfg->bus.spi, &tx);
    k_mutex_unlock(&data->lock);

    if (err < 0) {
        LOG_ERR("write failed %d", err);
    }
    return err;
}

/* Quad read: 0xEB + 3-byte addr + dummy (check datasheet) then data. Standard SPI API is single/dual;
 * full quad often needs SoC QSPI (e.g. RP2350 QMI). Stub sends cmd+addr and receives; controller must support quad.
 */
static int aps1604m_quad_read(const struct device* dev, off_t offset, void* buf, size_t len) {
    const struct aps1604m_config* cfg = dev->config;
    struct aps1604m_data* data = dev->data;

    if (cfg->is_qmi) {
        return -ENOTSUP;
    }
    uint8_t cmd[4];
    int err;

    if (offset + len > cfg->size_bytes || len == 0) {
        return -EINVAL;
    }

    cmd[0] = APS1604M_CMD_READ_QUAD;
    cmd[1] = (offset >> 16) & 0xFF;
    cmd[2] = (offset >> 8) & 0xFF;
    cmd[3] = offset & 0xFF;

    const struct spi_buf tx_buf = {.buf = cmd, .len = sizeof(cmd)};
    const struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
    const struct spi_buf rx_bufs[2] = {
        {.buf = NULL, .len = sizeof(cmd)},
        {.buf = buf, .len = len},
    };
    const struct spi_buf_set rx = {.buffers = rx_bufs, .count = ARRAY_SIZE(rx_bufs)};

    k_mutex_lock(&data->lock, K_FOREVER);
    err = spi_transceive_dt(&cfg->bus.spi, &tx, &rx);
    k_mutex_unlock(&data->lock);

    if (err < 0) {
        LOG_ERR("quad read failed %d", err);
    }
    return err;
}

/* Quad write: 0x38 + 3-byte addr + data. Controller must drive quad data lines if in quad mode. */
static int aps1604m_quad_write(const struct device* dev, off_t offset, const void* buf, size_t len) {
    const struct aps1604m_config* cfg = dev->config;
    struct aps1604m_data* data = dev->data;

    if (cfg->is_qmi) {
        return -ENOTSUP;
    }
    uint8_t cmd[4];
    int err;

    if (offset + len > cfg->size_bytes || len == 0) {
        return -EINVAL;
    }

    cmd[0] = APS1604M_CMD_WRITE_QUAD;
    cmd[1] = (offset >> 16) & 0xFF;
    cmd[2] = (offset >> 8) & 0xFF;
    cmd[3] = offset & 0xFF;

    const struct spi_buf tx_bufs[2] = {
        {.buf = cmd, .len = sizeof(cmd)},
        {.buf = (void*)buf, .len = len},
    };
    const struct spi_buf_set tx = {.buffers = tx_bufs, .count = ARRAY_SIZE(tx_bufs)};

    k_mutex_lock(&data->lock, K_FOREVER);
    err = spi_write_dt(&cfg->bus.spi, &tx);
    k_mutex_unlock(&data->lock);

    if (err < 0) {
        LOG_ERR("quad write failed %d", err);
    }
    return err;
}

__attribute__((unused)) static int aps1604_size(const struct device* dev) {
    const struct aps1604m_config* cfg = dev->config;
    return cfg->size_bytes;
}

static int aps1604m_rdid(const struct device* dev) {
    const struct aps1604m_config* cfg = dev->config;
    struct aps1604m_data* data = dev->data;
    uint8_t id[4];
    uint8_t cmd = APS1604M_CMD_READ_ID;
    int err;

    const struct spi_buf tx_buf = {.buf = &cmd, .len = sizeof(cmd)};
    const struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
    const struct spi_buf rx_buf = {.buf = id, .len = sizeof(id)};
    const struct spi_buf_set rx = {.buffers = &rx_buf, .count = 1};

    k_mutex_lock(&data->lock, K_FOREVER);
    err = spi_transceive_dt(&cfg->bus.spi, &tx, &rx);
    k_mutex_unlock(&data->lock);

    if (err < 0) {
        LOG_ERR("RDID failed %d", err);
        return err;
    }

    LOG_INF("APS1604M RDID: %02X %02X %02X %02X", id[0], id[1], id[2], id[3]);
    return 0;
}

int aps1604m_read_id(const struct device* dev, uint8_t* id, size_t len) {
    const struct aps1604m_config* cfg = dev->config;
    struct aps1604m_data* data = dev->data;

    if (cfg->is_qmi) {
        return -ENOTSUP;
    }
    uint8_t rdid[4];
    uint8_t cmd = APS1604M_CMD_READ_ID;
    int err;

    if (id == NULL || len == 0) {
        return -EINVAL;
    }
    if (len > 4) {
        len = 4;
    }

    const struct spi_buf tx_buf = {.buf = &cmd, .len = sizeof(cmd)};
    const struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
    const struct spi_buf rx_buf = {.buf = rdid, .len = sizeof(rdid)};
    const struct spi_buf_set rx = {.buffers = &rx_buf, .count = 1};

    k_mutex_lock(&data->lock, K_FOREVER);
    err = spi_transceive_dt(&cfg->bus.spi, &tx, &rx);
    k_mutex_unlock(&data->lock);

    if (err < 0) {
        return err;
    }
    memcpy(id, rdid, len);
    return 0;
}

int aps1604m_read_qspi(const struct device* dev, off_t offset, void* buf, size_t len) {
    return aps1604m_quad_read(dev, offset, buf, len);
}

int aps1604m_write_qspi(const struct device* dev, off_t offset, const void* buf, size_t len) {
    return aps1604m_quad_write(dev, offset, buf, len);
}

#define APS1604M_SPI_INIT(inst)                                                                                   \
    PINCTRL_DT_INST_DEFINE(inst);                                                                                 \
    static struct aps1604m_data aps1604m_data_##inst;                                                             \
    static const struct aps1604m_config aps1604m_config_##inst = {                                                \
        .is_qmi = false,                                                                                          \
        .bus.spi = SPI_DT_SPEC_INST_GET(inst, SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8)),           \
        .size_bytes = DT_INST_REG_SIZE(inst) ? DT_INST_REG_SIZE(inst) : DT_INST_PROP_OR(inst, size, 2097152),     \
        .spi_max_frequency = DT_INST_PROP_OR(inst, spi_max_frequency, 84000000),                                  \
        .pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(inst),                                                             \
    };                                                                                                            \
    DEVICE_DT_INST_DEFINE(inst, aps1604m_init, NULL, &aps1604m_data_##inst, &aps1604m_config_##inst, POST_KERNEL, \
                          CONFIG_PSRAM_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(APS1604M_SPI_INIT)

/*
 * When PSRAM is a child of the QMI flash controller (not an SPI controller),
 * use this compat so we don't use SPI_DT_SPEC. Transfer API returns -ENOTSUP until QMI path is implemented.
 */
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT aps_aps1604m_qmi
#define APS1604M_QMI_INIT(inst)                                                                               \
    PINCTRL_DT_INST_DEFINE(inst);                                                                             \
    static struct aps1604m_data aps1604m_data_qmi_##inst;                                                     \
    static const struct aps1604m_config aps1604m_config_qmi_##inst = {                                        \
        .is_qmi = true,                                                                                       \
        .bus.qmi.parent = DEVICE_DT_GET(DT_PARENT(DT_DRV_INST(inst))),                                        \
        .bus.qmi.cs = GPIO_DT_SPEC_INST_GET_BY_IDX(inst, cs_gpios, 0),                                        \
        .size_bytes = DT_INST_REG_SIZE(inst) ? DT_INST_REG_SIZE(inst) : DT_INST_PROP_OR(inst, size, 2097152), \
        .spi_max_frequency = DT_INST_PROP_OR(inst, spi_max_frequency, 84000000),                              \
        .pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(inst),                                                         \
    };                                                                                                        \
    DEVICE_DT_INST_DEFINE(inst, aps1604m_init, NULL, &aps1604m_data_qmi_##inst, &aps1604m_config_qmi_##inst,  \
                          POST_KERNEL, CONFIG_PSRAM_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(APS1604M_QMI_INIT)
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT aps_aps1604m
