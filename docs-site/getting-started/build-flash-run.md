# Building, Flashing, and Running the Reference Deployment

This guide assumes that the initial setup steps have been completed, and will walk through the steps of building, flashing, and running the reference deployment. This deployment implements an LED blinker component that is able to communicate with the fprime-gds.

## Building the Deployment
!!! note
    The flashing steps below run `make build`. It runs `fprime-util generate` when the build directory is missing, then `fprime-util build`, and also makes the `bootable.uf2` file you flash.

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

The supported boards are RP2350 flight controllers. You flash them by copying a UF2 file onto the board while it shows up as a USB drive. The [Board List][board-list] has the full steps and the checks for each step.

1. Find your board version (v5c, v5d, or v5e). It is printed on the board's silkscreen. If it is not v5e, change the board first. See [Specifying Board Configuration][specifying-board-configuration].

2. First time only (new board, or a board coming back from CircuitPython): build MCUboot, put the board into UF2 mode, and copy MCUboot to it. See [First time only: flash MCUboot][flash-mcuboot].

3. Build the signed application image, put the board into UF2 mode again, and copy the image to it. See [Every code change: flash the application][flash-app].

To put the board into UF2 mode, hold down BOOTSEL, press and release RESET, then release BOOTSEL. The board shows up as a USB drive named `RP2350`.

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
[flash-mcuboot]: ../additional-resources/board-list.md#first-time-only-flash-mcuboot
[flash-app]: ../additional-resources/board-list.md#every-code-change-flash-the-application
[specifying-board-configuration]: ../additional-resources/specifying-board-configuration.md
