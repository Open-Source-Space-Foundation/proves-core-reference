// ======================================================================
// \title  RAM.cpp
// \author ineskhou
// \brief  cpp file for RAM component implementation class
// ======================================================================
/*
 * APMemory APS1604M 16Mbit (2MB) QSPI PSRAM driver.
 *
 * Datasheet: power-ramp 150µs (board); driver adds 200µs before first SPI access.
 * Address A[20:0]; Enter Quad (0x35) in init for quad read/write.
 */

#define DT_DRV_COMPAT aps_aps1604m

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/spi.h>
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
#define MR0_OP0_Pos 0  // DQ_ZOUT
#define MR0_OP1_Pos 1  // DQ_ZOUT
#define MR0_OP2_Pos 2  // reserved
#define MR0_OP3_Pos 3  // reserved
#define MR0_OP4_Pos 4  // reserved
#define MR0_OP5_Pos 5  // wrap codes
#define MR0_OP6_Pos 6  // wrap codes
#define MR0_OP7_Pos \
    7  // reserved
       // namespace Components

#include "FprimeZephyrReference/Components/RAM/RAM.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

RAM ::RAM(const char* const compName) : RAMComponentBase(compName) {}

struct aps1604m_config {
    struct spi_dt_spec spi;
    size_t size_bytes;
    uint32_t spi_max_frequency;
    const struct pinctrl_dev_config* pcfg;
};

struct aps1604m_data {
    struct k_mutex lock;
};

RAM ::~RAM() {}

// Helper function to build the MR0 register (Mode Register 0)

// MR[1:0] is DQ_ZOUT is a 2-bit value that sets the output Drive Strength (MR[1:0])
// 00 is default (50 Ohms)
// 01 is 100 Ohms
// 10 is 200 Ohms
// 11 is reserved (page 12)

// MR[4:2] is reserved

// MR0[6:5] is Wrap Codes, used to wrap the read and write operations
// 00 is wrap 16, mp cross page bounc
// 01 is wrap 32, mp cross page boundary
// 10 is wrap 64, mp cross page boundary
// 11 is wrap 512 (page size),
//  with wrap CDMs (see latching truth table page 13)
// linear can cross page boundary, with Wrap CDMS, cannot

// MR0[7] is reserved

// When MA[3:0] is 0000 you are accessing MR0
static inline uint8_t build_mr0(uint8_t dq_zout, uint8_t wrap_codes) {
    // DQ_ZOUT 2 bits in MR[1:0]; Wrap Codes 2 bits in MR[6:5]
    return ((dq_zout & 0x03) << MR0_OP0_Pos) | ((wrap_codes & 0x03) << MR0_OP5_Pos);
}

static int aps1604m_reset(const struct device* dev) {
    const struct aps1604m_config* cfg = dev->config;
    struct aps1604m_data* data = dev->data;
    uint8_t rst_en = APS1604M_CMD_RESET_ENABLE;
    uint8_t rst = APS1604M_CMD_RESET;
    int err;

    // Reset must immediately follow Reset-Enable; no other command in between.
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

static int aps1604m_write_cmd(const struct device* dev, uint8_t cmd) {
    const struct aps1604m_config* cfg = dev->config;
    struct aps1604m_data* data = dev->data;
    const struct spi_buf tx_buf = {.buf = &cmd, .len = 1};
    const struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
    int err;
    k_mutex_lock(&data->lock, K_FOREVER);
    err = spi_write_dt(&cfg->spi, &tx);
    k_mutex_unlock(&data->lock);
    if (err < 0) {
        LOG_ERR("%s failed: %d", cmd, err);
        k_mutex_unlock(&data->lock);
        return err;
    }
    k_mutex_unlock(&data->lock);
    return 0;
}

static int aps1604m_init(const struct device* dev) {
    const struct aps1604m_config* cfg = dev->config;
    struct aps1604m_data* data = dev->data;
    int err;

    err = aps1604m_reset(dev);
    if (err < 0) {
        LOG_ERR("Reset failed %d", err);
        return err;
    }

    err = aps1604m_write_cmd(dev, APS1604M_CMD_ENTER_QUAD_MODE);
    if (err < 0) {
        LOG_ERR("Enter Quad Mode failed %d", err);
        return err;
    }
    return 0;
}

static int aps1604m_read_id(const struct device* dev, uint8_t* id, size_t len) {
    const struct aps1604m_config* cfg = dev->config;
    struct aps1604m_data* data = dev->data;
    uint8_t rd_id[4];
    int err;

    if (id == NULL || len != 4) {
        LOG_ERR("Invalid id or length %d, expected 4", len);
        return -EINVAL;
    }

    // need to use transieve to read the id
    const struct spi_buf tx_buf = {.buf = &cmd, .len = sizeof(cmd)};
    const struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
    const struct spi_buf rx_buf = {.buf = rdid, .len = sizeof(rdid)};
    const struct spi_buf_set rx = {.buffers = &rx_buf, .count = 1};

    k_mutex_lock(&data->lock, K_FOREVER);
    err = spi_transceive_dt(&cfg->spi, &tx, &rx);
    k_mutex_unlock(&data->lock);

    if (err < 0) {
        return err;
    }
    memcpy(id, rdid, len);
    return 0;
}
}  // namespace Components
