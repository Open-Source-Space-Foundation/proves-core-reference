# Building, Flashing, and Running the Reference Deployment

This guide assumes that the initial setup steps have been completed, and will walk through the steps of building, flashing, and running the reference deployment. This deployment implements an LED blinker component that is able to communicate with the fprime-gds.

## Building the Deployment
!!! note
    This step can be skipped if the setup.sh script is run. However, any changes made will require you to run `fprime-util build` and may require `fprime-util generate`

1. In order to build the ReferenceDeployment application, or any other F´ application, we first need to generate a build directory. This can be done with the following commands:

```sh
# In proves-core-reference (fprime-venv)
fprime-util generate
```

2. The next step is to build the ReferenceDeployment application's code.
```sh
# In proves-core-reference (fprime-venv)
fprime-util build
```

## Flashing the Board

The supported boards are RP2350 flight controllers. You flash them by copying a UF2 file onto the board while it is mounted as a USB drive. The [board list][board-list] covers each board, how to enter UF2 mode, and flashing over SWD.

1. First time only (new board, or a board coming back from CircuitPython): build MCUboot and copy it to the board.

    ```sh
    # In proves-core-reference
    make build-mcuboot
    cp mcuboot.uf2 [path-to-your-board]
    ```

2. Build the signed application image, put the board back into UF2 mode (see the [board list][board-list]), and copy the image to the board.

    ```sh
    # In proves-core-reference
    make build
    cp bootable.uf2 [path-to-your-board]
    ```

!!! note
    `fprime-util build` alone does not make `bootable.uf2`. `make build` runs `fprime-util build` and then converts the signed image. To convert it by hand:
    ```sh
    # In proves-core-reference
    ./tools/bin/make-loadable-image ./build-artifacts/zephyr.signed.bin bootable.uf2
    ```

!!! note
    One USB connection is enough to flash and to run the GDS. When the application runs, the board shows up as a USB CDC ACM serial device. The F Prime GDS connects to that port.

## Running the Deployment

The following command will spin up the F' GDS as well as run the application binary and the components necessary for the GDS and application to communicate.

```sh
# In proves-core-reference (fprime-venv)
fprime-gds -n --dictionary ./build-artifacts/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json --communication-selection uart --uart-baud 115200 --output-unframed-data

# Or
fprime-gds -n --dictionary ./build-artifacts/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json --communication-selection uart --uart-device /dev/cu.usbmodem142101 --uart-baud 115200

```

!!! note
    `/dev/cu.usbmodem142101` will likely need to be replaced with the correct port. This can be found by running the following command: `ls -l /dev/cu.usb*`


## Any Issues?
Refer to the [additional resources](../additional-resources/troubleshooting.md) for potential fixes.

<!-- Links -->
[board-list]: ../additional-resources/board-list.md
