/*
 * Board-specific QSPI/QMI pinctrl macros for RP2350.
 *
 * Zephyr's rpi-pico-rp2350a-pinctrl.h and rpi-pico-rp2350b-pinctrl.h only
 * define pins 0-29 (RP2350A) or 0-47 (RP2350B). The QSPI/QMI pins are
 * GPIO 55-60 per the RP2350 datasheet GPIO table; this header provides them.
 *
 * RP2350 datasheet: GPIO 55 = QSPI_SD3, 56 = QSPI_SCLK, 57 = QSPI_SD0,
 * 58 = QSPI_SD2, 59 = QSPI_SD1, 60 = QSPI_SS (QMI CS0n).
 * QSPI/QMI uses HSTX function (0) on these pins.
 */
#ifndef PROVES_FLIGHT_CONTROL_BOARD_V5_QSPI_PINCTRL_H_
#define PROVES_FLIGHT_CONTROL_BOARD_V5_QSPI_PINCTRL_H_

#include <zephyr/dt-bindings/pinctrl/rpi-pico-rp2350a-pinctrl.h>

/* QSPI/QMI uses HSTX (0) on RP2350 for dedicated QSPI pins */
#define RP2_PINCTRL_GPIO_FUNC_QSPI RP2_PINCTRL_GPIO_FUNC_HSTX

/* QSPI clock */
#define QSPI_SCK_P56 RP2XXX_PINMUX(56, RP2_PINCTRL_GPIO_FUNC_QSPI)

/* QSPI data lines (Quad: SD0–SD3) */
#define QSPI_SD0_P57 RP2XXX_PINMUX(57, RP2_PINCTRL_GPIO_FUNC_QSPI)
#define QSPI_SD1_P59 RP2XXX_PINMUX(59, RP2_PINCTRL_GPIO_FUNC_QSPI)
#define QSPI_SD2_P58 RP2XXX_PINMUX(58, RP2_PINCTRL_GPIO_FUNC_QSPI)
#define QSPI_SD3_P55 RP2XXX_PINMUX(55, RP2_PINCTRL_GPIO_FUNC_QSPI)

#endif /* PROVES_FLIGHT_CONTROL_BOARD_V5_QSPI_PINCTRL_H_ */
