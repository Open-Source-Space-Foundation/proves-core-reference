/*
 * Copyright (c) 2025 BroncoSpace / PROVES Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file memc_rp2350_psram.h
 * @brief Public API for the RP2350 QMI PSRAM driver (APS1604M).
 *
 * After a successful POST_KERNEL init the PSRAM is accessible as ordinary
 * memory at [psram_base(), psram_base() + psram_size()).  Callers must not
 * touch this region until psram_is_ready() returns true.
 *
 * The XIP CS1 window base (0x11000000) and 2 MB size come from the
 * devicetree node; these accessors are the canonical way to obtain them
 * without hard-coding the values in application code.
 */

#ifndef DRIVERS_MEMC_MEMC_RP2350_PSRAM_H_
#define DRIVERS_MEMC_MEMC_RP2350_PSRAM_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Return the base address of the PSRAM XIP CS1 window.
 *
 * @return Physical base address (0x11000000 for APS1604M on RP2350 CS1),
 *         or 0 if the driver is not enabled.
 */
uintptr_t psram_base(void);

/**
 * @brief Return the size of the PSRAM region in bytes.
 *
 * @return Size in bytes (2 MB = 2097152 for APS1604M),
 *         or 0 if the driver is not enabled.
 */
size_t psram_size(void);

/**
 * @brief Return the base of the uncached, non-allocating alias of the PSRAM.
 *
 * Accesses through this alias bypass the (write-back) XIP cache and always
 * hit the chip — required for memtests.  Same size as psram_size().
 *
 * @return Uncached alias base (0x15000000 on RP2350 CS1),
 *         or 0 if the driver is not enabled.
 */
uintptr_t psram_nocache_base(void);

/**
 * @brief Check whether the PSRAM init sequence completed successfully.
 *
 * Returns false if the PSRAM was not detected (bad KGD byte in RDID)
 * or if the driver is not enabled.  Flight code should check this before
 * using the PSRAM heap.
 *
 * @return true  PSRAM is present and ready.
 * @return false PSRAM not detected or init failed.
 */
bool psram_is_ready(void);

/**
 * @brief Check whether the PSRAM shared_multi_heap pool is usable.
 *
 * True once the region has been added to the shared_multi_heap pool and a
 * trial allocation succeeded.  Allocate with
 * shared_multi_heap_alloc(SMH_REG_ATTR_EXTERNAL, size).
 *
 * @return true PSRAM heap verified at init.
 */
bool psram_heap_ok(void);

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_MEMC_MEMC_RP2350_PSRAM_H_ */
