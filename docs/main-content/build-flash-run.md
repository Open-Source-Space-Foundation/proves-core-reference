# Building, Flashing, and Running the Reference Deployment

This guide assumes that the initial setup steps have been completed, and will walk through the steps of building, flashing, and running the reference deployment. This deployment implements an LED blinker component that is able to communicate with the fprime-gds.

## Building the Deployment
> [!Note]
> This step can be skipped if the setup.sh script is run. However, any changes made will require you to run `fprime-util build` and may require `fprime-util generate`

1. In order to build the ReferenceDeployment application, or any other F´ application, we first need to generate a build directory. This can be done with the following commands:

```sh
# In fprime-zephyr-reference (fprime-venv)
fprime-util generate
```

2. The next step is to build the ReferenceDeployment application's code.
```sh
# In fprime-zephyr-reference (fprime-venv)
fprime-util build
```

## Flashing the Board

Different boards will likely require different steps to flash software onto the board. Instructions for tested boards are provided [here][board-list] The following is an example of flashing the deployment onto a stm32 board.

```sh
# In fprime-zephyr-reference

# Linux/Windows WSL
sh ~/.arduino15/packages/STMicroelectronics/tools/STM32Tools/2.3.0/stm32CubeProg.sh -i swd -f build-fprime-automatic-zephyr/zephyr/zephyr.hex -c /dev/ttyACM0

# MacOS
sh ~/Library/Arduino15/packages/STMicroelectronics/tools/STM32Tools/2.3.0/stm32CubeProg.sh -i swd -f build-fprime-automatic-zephyr/zephyr/zephyr.hex -c /dev/cu.usbmodem142203
```

> [!Note]
> Change `/dev/ttyACM0` (`/dev/cu.usbmodem141203` for MacOS) to the correct serial device connected to the device. To find the correct serial port, refer to this documentation [here](https://github.com/ngcp-project/gcs-infrastructure/blob/d34eeba4eb547a5174d291a64b36eaa8c11369c8/Communication/XBee/docs/serial_port.md)

> [!Note]
> Two USB connections may be required depending on the board configuration. USB PWR is used to power and flash the development board and access the debug terminal, USER USB is used to connect to the F Prime GDS

## Running the Deployment

The following command will spin up the F' GDS as well as run the application binary and the components necessary for the GDS and application to communicate.

```sh
# In fprime-zephyr-reference (fprime-venv)
fprime-gds -n --dictionary ./build-artifacts/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json --communication-selection uart --uart-baud 115200 --output-unframed-data

# Or
fprime-gds -n --dictionary ./build-artifacts/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json --communication-selection uart --uart-device /dev/cu.usbmodem142101 --uart-baud 115200

```

> [!Note]
> `/dev/cu.usbmodem142101` will likely need to be replaced with the correct port. This can be found by running the following command: `ls -l /dev/cu.usb*`


## Any Issues?
Refer to the [additional resources][additional-resources] section in the main README file for potential fixes.

<!-- Links -->
[board-list]: ../additional-resources/board-list.md
[additional-resources]: ../../README.md#additional-resources
