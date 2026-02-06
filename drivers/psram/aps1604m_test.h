/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * PSRAM (APS1604M) test invoked from main.
 */

#ifndef APS1604M_TEST_H_
#define APS1604M_TEST_H_

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Run basic read-ID and read/write test on the given PSRAM device. */
void aps1604m_test(const struct device* dev);

#ifdef __cplusplus
}
#endif

#endif /* APS1604M_TEST_H_ */
