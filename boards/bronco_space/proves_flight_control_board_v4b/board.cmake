# SPDX-License-Identifier: Apache-2.0

if("${RPI_PICO_DEBUG_ADAPTER}" STREQUAL "")
	set(RPI_PICO_DEBUG_ADAPTER "cmsis-dap")
endif()

board_runner_args(openocd --cmd-pre-init "source [find interface/${RPI_PICO_DEBUG_ADAPTER}.cfg]")



# The adapter speed is expected to be set by interface configuration.
# The Raspberry Pi's OpenOCD fork doesn't, so match their documentation at
# https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html#debugging-with-swd
board_runner_args(openocd --cmd-pre-init "set_adapter_speed_if_not_set 5000")

if(BOARD MATCHES "^proves_flight_control_board_v4")
    board_runner_args(openocd --cmd-pre-init "source [find target/rp2040.cfg]")
    board_runner_args(jlink "--device=RP2040_M0_0")
    board_runner_args(uf2 "--board-id=RP2040")
elseif(BOARD MATCHES "^proves_flight_control_board_v5")
    board_runner_args(openocd --cmd-pre-init "source [find target/rp2340.cfg]")
    board_runner_args(jlink "--device=RP2340_M0_0")
    board_runner_args(uf2 "--board-id=RP2340")
endif()




include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
include(${ZEPHYR_BASE}/boards/common/uf2.board.cmake)
