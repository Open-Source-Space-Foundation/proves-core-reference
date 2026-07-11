/*
 * Copyright (c) 2025 BroncoSpace / PROVES Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * RP2350 QMI PSRAM driver — APMemory APS1604M-3SQR
 * Issue #285, slices S2 + S3.
 *
 * Architecture notes
 * ------------------
 * The APS1604M is wired to QMI chip-select 1 (XIP CS1, base 0x11000000).
 * After this one-shot init the chip is memory-mapped; no further driver
 * calls are needed for read/write access.
 *
 * The entire QMI direct-mode window (psram_setup_ramfunc) runs from RAM
 * with interrupts locked.  While DIRECT_CSR.EN is set, any memory-mapped
 * access through the QMI (including instruction fetch from flash) stalls.
 * Running from flash at that point causes a bus hang and a watchdog reset —
 * this is exactly the failure mode observed on the pico-ram branch.
 *
 * __ramfunc is available on ARM Cortex-M because CONFIG_ARCH_HAS_RAMFUNC_SUPPORT
 * is always selected for ARM in Zephyr (arch/arm/core/Kconfig).  On XIP
 * builds it places the function in the .ramfunc linker section and applies
 * long_call so the linker inserts a veneer if needed.
 *
 * Reference: RP2350 datasheet §12.14; SparkFun sfe_psram.c (public).
 */

#include "memc_rp2350_psram.h"

#include <hardware/regs/qmi.h>
#include <hardware/structs/qmi.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(memc_rp2350_psram, CONFIG_LOG_DEFAULT_LEVEL);

/* ---- DT node ------------------------------------------------------------ */

#define DT_DRV_COMPAT raspberrypi_rp2350_psram_aps1604m

/* ---- APS1604M command bytes --------------------------------------------- */
#define PSRAM_CMD_EXIT_QPI 0xF5u /* exit quad-peripheral-interface mode */
#define PSRAM_CMD_RESET_ENABLE 0x66u
#define PSRAM_CMD_RESET 0x99u
#define PSRAM_CMD_READ_ID 0x9Fu
#define PSRAM_CMD_ENTER_QUAD 0x35u

/* Expected RDID response bytes */
#define PSRAM_MF_ID_EXPECTED 0x0Du /* APMemory manufacturer ID */
#define PSRAM_KGD_PASS 0x5Du       /* Known Good Die */

/* tRST: min 50 µs after software reset before issuing any command. */
#define PSRAM_TRST_CYCLES 10000u /* conservative; ~100 µs at 100 MHz */

/* ---- QMI helpers -------------------------------------------------------- */

/*
 * Conservative clock divider for direct mode.  Actual sys_clk / (2*30) ~
 * 2.5 MHz — well within spec for SPI mode during reset/RDID.
 */
#define PSRAM_DIRECT_CLKDIV 30u

/* Bit shortcuts (from hardware/regs/qmi.h) */
#define DIRECT_CSR_EN QMI_DIRECT_CSR_EN_BITS
#define DIRECT_CSR_BUSY QMI_DIRECT_CSR_BUSY_BITS
#define DIRECT_CSR_TXEMPTY QMI_DIRECT_CSR_TXEMPTY_BITS
#define DIRECT_CSR_RXEMPTY QMI_DIRECT_CSR_RXEMPTY_BITS
#define DIRECT_CSR_ASSERT_CS1N QMI_DIRECT_CSR_ASSERT_CS1N_BITS

#define DIRECT_TX_OE QMI_DIRECT_TX_OE_BITS
#define DIRECT_TX_NOPUSH QMI_DIRECT_TX_NOPUSH_BITS
#define DIRECT_TX_IWIDTH_Q (QMI_DIRECT_TX_IWIDTH_VALUE_Q << QMI_DIRECT_TX_IWIDTH_LSB)

/* ---- Driver state ------------------------------------------------------- */

struct psram_config {
    uintptr_t base;  /* XIP CS1 window base address */
    size_t size;     /* window size in bytes        */
    uint32_t max_hz; /* max-frequency from DT       */
    const struct pinctrl_dev_config* pcfg;
};

struct psram_data {
    bool ready; /* true after successful RDID */
};

/* ---- RAM-resident setup routine (S3) ------------------------------------ */

/*
 * ID bytes returned by the RDID command: [MF_ID, KGD, EID0..EID5, density]
 * We capture all 8 to give the caller enough to log.
 */
struct psram_id {
    uint8_t mf_id;
    uint8_t kgd;
    uint8_t eid[6];
};

/*
 * Busy-wait loop that does NOT call any external function (all loops must
 * be inlined or be __ramfunc themselves to avoid flash access).
 * Volatile prevents the compiler from eliminating the loop.
 */
static __ramfunc void psram_delay_cycles(volatile uint32_t n) {
    while (n--) {
        /* intentional empty spin */
        __asm__ volatile("nop");
    }
}

/*
 * Push one byte to the QMI DIRECT_TX FIFO and wait until the transfer
 * completes.  iwidth selects SPI (0) or Quad (DIRECT_TX_IWIDTH_Q).
 * Set nopush if you do not want the corresponding RX byte queued.
 */
static __ramfunc void psram_tx(uint32_t iwidth, uint8_t data, bool oe, bool nopush) {
    uint32_t entry = (uint32_t)data;

    entry |= iwidth;
    if (oe) {
        entry |= DIRECT_TX_OE;
    }
    if (nopush) {
        entry |= DIRECT_TX_NOPUSH;
    }

    qmi_hw->direct_tx = entry;
    /* wait for TX to drain and transfer to finish */
    while (!(qmi_hw->direct_csr & DIRECT_CSR_TXEMPTY)) {
        /* spin */
    }
    while (qmi_hw->direct_csr & DIRECT_CSR_BUSY) {
        /* spin */
    }
}

/*
 * Drain one byte from the RX FIFO (must have been received by a prior tx
 * without NOPUSH).
 */
static __ramfunc uint8_t psram_rx_byte(void) {
    while (qmi_hw->direct_csr & DIRECT_CSR_RXEMPTY) {
        /* spin */
    }
    return (uint8_t)(qmi_hw->direct_rx & 0xFFu);
}

/*
 * psram_setup_ramfunc — the entire QMI direct-mode window.
 *
 * Must be __ramfunc; must not call any function that is not __ramfunc or
 * static-inline.  No LOG_*, no string literals, no non-inline function calls.
 * The caller holds irq_lock().
 *
 * Returns a filled psram_id struct.  The caller checks kgd after returning.
 */
static __ramfunc struct psram_id psram_setup_ramfunc(void) {
    struct psram_id id = {0};
    uint32_t save_csr;
    uint32_t csr;

    /* 1. Save current DIRECT_CSR and enable direct mode with clkdiv ~30. */
    save_csr = qmi_hw->direct_csr;
    csr = (PSRAM_DIRECT_CLKDIV << QMI_DIRECT_CSR_CLKDIV_LSB) | DIRECT_CSR_EN;
    qmi_hw->direct_csr = csr;
    /* wait until not busy */
    while (qmi_hw->direct_csr & DIRECT_CSR_BUSY) {
        /* spin */
    }

    /* 2. Assert CS1n. */
    qmi_hw->direct_csr |= DIRECT_CSR_ASSERT_CS1N;

    /*
     * 3. Exit-QPI (0xF5) in quad mode.
     *    The chip may have been left in QPI mode from a warm boot.
     *    Send it regardless; in SPI mode the chip ignores unrecognised
     *    quad cycles gracefully.
     */
    psram_tx(DIRECT_TX_IWIDTH_Q, PSRAM_CMD_EXIT_QPI,
             /*oe=*/true, /*nopush=*/true);

    /* 4. Deassert CS1n (end of Exit-QPI transfer). */
    qmi_hw->direct_csr &= ~DIRECT_CSR_ASSERT_CS1N;
    while (qmi_hw->direct_csr & DIRECT_CSR_BUSY) {
        /* spin */
    }

    /* 5. Assert CS1n for Reset-Enable + Reset in SPI mode. */
    qmi_hw->direct_csr |= DIRECT_CSR_ASSERT_CS1N;

    /* Reset-Enable 0x66 (SPI, OE, NOPUSH) */
    psram_tx(0u, PSRAM_CMD_RESET_ENABLE, /*oe=*/true, /*nopush=*/true);

    /* 6. Deassert CS1n between Reset-Enable and Reset (per datasheet). */
    qmi_hw->direct_csr &= ~DIRECT_CSR_ASSERT_CS1N;
    while (qmi_hw->direct_csr & DIRECT_CSR_BUSY) {
        /* spin */
    }

    /* 7. Assert CS1n for Reset command. */
    qmi_hw->direct_csr |= DIRECT_CSR_ASSERT_CS1N;

    /* Reset 0x99 (SPI, OE, NOPUSH) */
    psram_tx(0u, PSRAM_CMD_RESET, /*oe=*/true, /*nopush=*/true);

    /* 8. Deassert CS1n, then wait tRST (>50 µs). */
    qmi_hw->direct_csr &= ~DIRECT_CSR_ASSERT_CS1N;
    while (qmi_hw->direct_csr & DIRECT_CSR_BUSY) {
        /* spin */
    }
    psram_delay_cycles(PSRAM_TRST_CYCLES);

    /* 9. Assert CS1n for Read ID. */
    qmi_hw->direct_csr |= DIRECT_CSR_ASSERT_CS1N;

    /* READ_ID 0x9F (SPI, OE, NOPUSH) */
    psram_tx(0u, PSRAM_CMD_READ_ID, /*oe=*/true, /*nopush=*/true);

    /*
     * 3 address bytes (0x00 0x00 0x00) — required by the command format.
     * OE=true (we drive the bus), NOPUSH (discard the dummy clocks).
     */
    psram_tx(0u, 0x00u, /*oe=*/true, /*nopush=*/true);
    psram_tx(0u, 0x00u, /*oe=*/true, /*nopush=*/true);
    psram_tx(0u, 0x00u, /*oe=*/true, /*nopush=*/true);

    /*
     * Clock in 8 ID bytes — OE=false so we tri-state SD0 and sample SDx.
     * NOPUSH=false so bytes land in the RX FIFO.
     */
    psram_tx(0u, 0x00u, /*oe=*/false, /*nopush=*/false); /* MF_ID */
    psram_tx(0u, 0x00u, /*oe=*/false, /*nopush=*/false); /* KGD   */
    psram_tx(0u, 0x00u, /*oe=*/false, /*nopush=*/false); /* EID0  */
    psram_tx(0u, 0x00u, /*oe=*/false, /*nopush=*/false); /* EID1  */
    psram_tx(0u, 0x00u, /*oe=*/false, /*nopush=*/false); /* EID2  */
    psram_tx(0u, 0x00u, /*oe=*/false, /*nopush=*/false); /* EID3  */
    psram_tx(0u, 0x00u, /*oe=*/false, /*nopush=*/false); /* EID4  */
    psram_tx(0u, 0x00u, /*oe=*/false, /*nopush=*/false); /* EID5  */

    /* Drain the RX FIFO — order must match the pushes above. */
    id.mf_id = psram_rx_byte();
    id.kgd = psram_rx_byte();
    id.eid[0] = psram_rx_byte();
    id.eid[1] = psram_rx_byte();
    id.eid[2] = psram_rx_byte();
    id.eid[3] = psram_rx_byte();
    id.eid[4] = psram_rx_byte();
    id.eid[5] = psram_rx_byte();

    /* 10. Deassert CS1n after Read ID. */
    qmi_hw->direct_csr &= ~DIRECT_CSR_ASSERT_CS1N;
    while (qmi_hw->direct_csr & DIRECT_CSR_BUSY) {
        /* spin */
    }

    /* 11. Assert CS1n for Enter Quad Mode (0x35). */
    qmi_hw->direct_csr |= DIRECT_CSR_ASSERT_CS1N;
    psram_tx(0u, PSRAM_CMD_ENTER_QUAD, /*oe=*/true, /*nopush=*/true);

    /* 12. Deassert CS1n — chip is now in QPI mode. */
    qmi_hw->direct_csr &= ~DIRECT_CSR_ASSERT_CS1N;
    while (qmi_hw->direct_csr & DIRECT_CSR_BUSY) {
        /* spin */
    }

    /* 13. Restore DIRECT_CSR (clears EN, returns to XIP-controlled mode). */
    qmi_hw->direct_csr = save_csr;

    return id;
}

/* ---- POST_KERNEL init --------------------------------------------------- */

static int psram_init(const struct device* dev) {
    const struct psram_config* cfg = dev->config;
    struct psram_data* data = dev->data;
    int ret;

    /* Apply pinctrl: mux GPIO8 to XIP_CS1n (alt function 9). */
    ret = pinctrl_apply_state(cfg->pcfg, PINCTRL_STATE_DEFAULT);
    if (ret < 0) {
        LOG_ERR("psram: pinctrl failed: %d", ret);
        return ret;
    }

    LOG_INF("psram: init (base=0x%08x size=%zu max_hz=%u)", (uint32_t)cfg->base, cfg->size, cfg->max_hz);

    /* Run the direct-mode sequence from RAM with interrupts locked. */
    unsigned int key = irq_lock();
    struct psram_id id = psram_setup_ramfunc();
    irq_unlock(key);

    /* Parse and log ID bytes — safe to do after irq_unlock(). */
    LOG_INF(
        "psram: RDID mf_id=0x%02x kgd=0x%02x "
        "eid=%02x%02x%02x%02x%02x%02x",
        id.mf_id, id.kgd, id.eid[0], id.eid[1], id.eid[2], id.eid[3], id.eid[4], id.eid[5]);

    if (id.mf_id != PSRAM_MF_ID_EXPECTED || id.kgd != PSRAM_KGD_PASS) {
        LOG_ERR(
            "psram: bad RDID (mf_id=0x%02x kgd=0x%02x) — "
            "chip not detected or not a known-good die; "
            "boot continues without PSRAM",
            id.mf_id, id.kgd);
        data->ready = false;
        /*
         * Return 0 (not -ENODEV) so the kernel does not treat this
         * as a fatal init error.  The device is left initialised but
         * psram_is_ready() returns false; consumers must check.
         */
        return 0;
    }

    /* Cross-check: density bits in EID[0] bits[5:4] = 0b10 → 64 Mbit.
     * For APS1604M that is 2 MB — matches DT size.  Just log, don't fail.
     */
    uint8_t density = (id.eid[0] >> 4) & 0x3u;

    if (density != 0x2u) {
        LOG_WRN("psram: unexpected density bits 0x%x (expected 0x2 for 2MB)", density);
    }

    data->ready = true;
    LOG_INF("psram: ready — 2 MB at 0x%08x", (uint32_t)cfg->base);

    return 0;
}

/* ---- Public API --------------------------------------------------------- */

static const struct device* psram_dev_ptr;

uintptr_t psram_base(void) {
    if (!psram_dev_ptr || !device_is_ready(psram_dev_ptr)) {
        return 0;
    }
    const struct psram_config* cfg = psram_dev_ptr->config;

    return cfg->base;
}

size_t psram_size(void) {
    if (!psram_dev_ptr || !device_is_ready(psram_dev_ptr)) {
        return 0;
    }
    const struct psram_config* cfg = psram_dev_ptr->config;

    return cfg->size;
}

bool psram_is_ready(void) {
    if (!psram_dev_ptr || !device_is_ready(psram_dev_ptr)) {
        return false;
    }
    const struct psram_data* data = psram_dev_ptr->data;

    return data->ready;
}

/* ---- Device definition -------------------------------------------------- */

#define PSRAM_INST_DEFINE(inst)                                                                          \
    PINCTRL_DT_INST_DEFINE(inst);                                                                        \
    static const struct psram_config psram_config_##inst = {                                             \
        .base = DT_INST_REG_ADDR(inst),                                                                  \
        .size = DT_INST_REG_SIZE(inst),                                                                  \
        .max_hz = DT_INST_PROP(inst, max_frequency),                                                     \
        .pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(inst),                                                    \
    };                                                                                                   \
    static struct psram_data psram_data_##inst;                                                          \
    DEVICE_DT_INST_DEFINE(inst, psram_init, NULL, &psram_data_##inst, &psram_config_##inst, POST_KERNEL, \
                          CONFIG_MEMC_RP2350_PSRAM_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(PSRAM_INST_DEFINE)

/*
 * Store a pointer to the (single) device instance so the psram_base/size/
 * is_ready accessors can retrieve it without the caller having access to a
 * struct device pointer.
 */
static int psram_store_dev_ptr(void) {
    /* Only instance 0 is expected; the macro above handles all instances. */
#define PSRAM_STORE(inst) psram_dev_ptr = DEVICE_DT_INST_GET(inst);
    DT_INST_FOREACH_STATUS_OKAY(PSRAM_STORE)
#undef PSRAM_STORE
    return 0;
}

/*
 * Priority 71: one after the device init at 70, so psram_dev_ptr is set
 * before any consumer's POST_KERNEL init at priority > 71.
 * Must be a literal integer, not an expression — Zephyr encodes the
 * priority into the section name at compile time.
 */
SYS_INIT(psram_store_dev_ptr, POST_KERNEL, 71);
