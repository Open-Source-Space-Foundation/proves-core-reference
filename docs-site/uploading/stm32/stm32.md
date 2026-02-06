# Uploading Guide for STM32 Chips

This guide sets up `arduino-cli` to flash code onto stm32 boards. If you do not want to use the `arduino-cli`, the STM32CubeProgrammer UI application can be used.

These steps were tested for the following boards:

- NUCLEO-H723ZG
- STM32H753I-EVAL

## Initial Setup
1. Follow the `arduino-cli` installation guide and install the board package for the `STMicroelectronics:stm32`
2. Install the [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html`). More information can be found [here](https://github.com/stm32duino/Arduino_Core_STM32/wiki/Upload-methods)

> [!Warning]
> If you installed the ARM version of the STM32CubeProgrammer, please follow the steps provided [here](./Arduino-STM32CubeProgrammer-Arm-Issue.md)

## Flashing the Board
```sh
# In fprime-stm32h7-zephyr-reference

# Linux/Windows WSL
sh ~/.arduino15/packages/STMicroelectronics/tools/STM32Tools/2.3.0/stm32CubeProg.sh -i swd -f build-fprime-automatic-zephyr/zephyr/zephyr.hex -c /dev/ttyACM0

# MacOS
sh ~/Library/Arduino15/packages/STMicroelectronics/tools/STM32Tools/2.3.0/stm32CubeProg.sh -i swd -f build-fprime-automatic-zephyr/zephyr/zephyr.hex -c /dev/cu.usbmodem142203
```

> [!Note]
> Change `/dev/ttyACM0` (`/dev/cu.usbmodem141203` for MacOS) to the correct serial device connected to the device. To find the correct serial port, refer to this documentation [here](https://github.com/ngcp-project/gcs-infrastructure/blob/d34eeba4eb547a5174d291a64b36eaa8c11369c8/Communication/XBee/docs/serial_port.md)
