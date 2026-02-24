/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * RP2350A dedicated QSPI pins (package pins 55–60).
 * Not in upstream rpi-pico-rp2350* pinctrl because they are fixed-function
 * QSPI pads, not muxable GPIOs. This board overlay defines them for use with
 * qspi_default when driving PSRAM (e.g. APS1604M) on the QMI bus.
 *
 * Pin numbers match RP2350 pad indices for the QSPI block (see rp2350a.pinout.xyz).
 * Alt func 0 = QSPI (QMI) per RP2350 pad default.
 */
#ifndef PROVES_FLIGHT_CONTROL_BOARD_V5_QSPI_PINCTRL_H_
#define PROVES_FLIGHT_CONTROL_BOARD_V5_QSPI_PINCTRL_H_

#include <zephyr/dt-bindings/pinctrl/rpi-pico-rp2350a-pinctrl.h>

/* QSPI pad function: dedicated QSPI (QMI); not in common pinctrl. */
#define RP2_PINCTRL_GPIO_FUNC_QSPI 0

/* Package pins 55–60: QSPI_SD3, QSPI_SCLK, QSPI_SD0, QSPI_SD2, QSPI_SD1, QSPI_SS */
#define QSPI_SD3_P55 RP2XXX_PINMUX(55, RP2_PINCTRL_GPIO_FUNC_QSPI)
#define QSPI_SCK_P56 RP2XXX_PINMUX(56, RP2_PINCTRL_GPIO_FUNC_QSPI)
#define QSPI_SD0_P57 RP2XXX_PINMUX(57, RP2_PINCTRL_GPIO_FUNC_QSPI)
#define QSPI_SD2_P58 RP2XXX_PINMUX(58, RP2_PINCTRL_GPIO_FUNC_QSPI)
#define QSPI_SD1_P59 RP2XXX_PINMUX(59, RP2_PINCTRL_GPIO_FUNC_QSPI)
#define QSPI_SS_P60 RP2XXX_PINMUX(60, RP2_PINCTRL_GPIO_FUNC_QSPI)

/* RP2350 GPIO8: pad function 9 = XIP_CS1n (QMI chip select 1 for PSRAM). */
#define XIP_CS1_P8 RP2XXX_PINMUX(8, 9)

#endif /* PROVES_FLIGHT_CONTROL_BOARD_V5_QSPI_PINCTRL_H_ */
