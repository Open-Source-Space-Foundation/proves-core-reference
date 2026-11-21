/*
 * Copyright (c) 2025 BroncoSpace / PROVES Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * RP2350A dedicated QSPI pins (package pins 55-60).
 * Not in upstream rpi-pico-rp2350* pinctrl because they are fixed-function
 * QSPI pads, not muxable GPIOs.
 *
 * The QSPI data/clock pads (pins 55-60) are fixed-function — the QMI block
 * owns them permanently and they do not appear in the standard GPIO bank.
 * They are listed here as documentation only; no pinctrl entry is needed
 * for them (the QMI flash driver never configures them via pinctrl either).
 *
 * XIP_CS1_P8 is the only functional macro: GPIO8 muxed to alt-function 9
 * (XIP_CS1n) selects the PSRAM on QMI chip-select 1.
 *
 * Note on function 9 vs Zephyr header naming:
 *   Zephyr's rpi-pico-rp2350-pinctrl-common.h labels function 9 as
 *   RP2_PINCTRL_GPIO_FUNC_GPCK (generic clock output).  On GPIO8 specifically
 *   the RP2350 datasheet (Table 8) assigns alt-function 9 = XIP_CS1n.
 *   The pinctrl driver passes the raw function number to gpio_set_function()
 *   in the HAL, which routes it correctly regardless of the Zephyr macro name.
 *   RP2XXX_PINMUX(8, 9) therefore correctly configures GPIO8 as XIP_CS1n.
 */
#ifndef PROVES_FLIGHT_CONTROL_BOARD_V5_QSPI_PINCTRL_H_
#define PROVES_FLIGHT_CONTROL_BOARD_V5_QSPI_PINCTRL_H_

#include <zephyr/dt-bindings/pinctrl/rpi-pico-rp2350a-pinctrl.h>

/*
 * Documentation-only macros for the fixed-function QSPI pads.
 * These pads are permanently owned by the QMI block and cannot be
 * reassigned. No pinctrl node should reference them.
 *
 * #define QSPI_SD3_P55  RP2XXX_PINMUX(55, 0)
 * #define QSPI_SCK_P56  RP2XXX_PINMUX(56, 0)
 * #define QSPI_SD0_P57  RP2XXX_PINMUX(57, 0)
 * #define QSPI_SD2_P58  RP2XXX_PINMUX(58, 0)
 * #define QSPI_SD1_P59  RP2XXX_PINMUX(59, 0)
 * #define QSPI_SS_P60   RP2XXX_PINMUX(60, 0)
 */

/*
 * GPIO8 -> XIP_CS1n: RP2350 alt function 9 on GPIO8 routes the pad to the
 * QMI chip-select 1 output. Used to enable the APS1604M PSRAM on v5d boards.
 */
#define XIP_CS1_P8 RP2XXX_PINMUX(8, 9)

#endif /* PROVES_FLIGHT_CONTROL_BOARD_V5_QSPI_PINCTRL_H_ */
