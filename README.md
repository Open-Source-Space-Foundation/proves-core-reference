# Proves Core Reference Project

This is a reference software implementation for the [PROVES Kit](https://docs.proveskit.space/en/latest/).

## Documentation

📚 **[Component Software Design Documents (SDDs)](https://open-source-space-foundation.github.io/proves-core-reference/)** - Browse detailed design documentation for all components.

## System Requirements
- F Prime System Requirements listed [here](https://fprime.jpl.nasa.gov/latest/docs/getting-started/installing-fprime/#system-requirements)
- Zephyr dependencies listed [here](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#install-dependencies) (NOTE: Only complete the install dependencies step, as we run through the rest of the steps in this readme.)

## Installation

First, clone the Proves Core Reference repository.

```shell
git clone https://github.com/Open-Source-Space-Foundation/proves-core-reference
```

Next, navigate to the `proves-core-reference` directory and run `make` to set up the project.

```shell
cd proves-core-reference
make
```

### Bootloader (MCUBoot)
If you have a brand new flight controller board (or are switching a board back to F Prime from CircuitPython) you will need to first install the MCUBoot Bootloader. Skip this step if you are already running F Prime on the board at V1.0.0+.

Build the MCUBoot bootloader:

```shell
make build-mcuboot
```

This creates a bootloader with two partitions (slots) so the system can swap between images for over-the-air updates.

### Find the path to your board

Next, plug in your board! If you have previously installed a firmware on your board you may not see it show up as a drive. In that case you'll want to put the board into boot loader mode. Then you'll be able to find the location of the board on your computer. It should be called something like RP2350 but you want to find the path to it

For Mac:
```shell
ls -lah /Volumes
```

For Windows:
Check the letter said to be the mount (ex /d/) and then the name of the removable drive (ex /d/RP2350)

For Linux:
```shell
findmnt
```


#### Copy/flash the bootloader to the board

The bootloader build outputs `mcuboot.uf2` at the repo root Put the board into UF2 bootloader mode (so it mounts as a USB drive), then copy it onto the mounted drive:

```shell
cp mcuboot.uf2 [path-to-your-board]
```

## Running the code

Run generate from the `proves-core-reference` directory. This generates the build cache for FPrime. You only need to do generate if something in the core FPrime package has changed
```shell
make generate
```

Then, and every time you change code, run

```shell
make build
```

Now you want to install the firmware onto the board.
```shell
cp bootable.uf2 [path-to-your-board]
```

If this is your first time running the gds, you must create the authentication plug:
```shell
make framer-plugin
```

Finally, run the fprime-gds.
```shell
make gds
```

#### Ensuring your authentication/signing is correct

The Makefile will ensure the authentication is correct if you run the code on the same computer you flash on. However, if you switch from a computer that compiled the code you will likely have issues with authentication. Here are some things you may encounter

MCUBoot only boots images that are **signed with the same key** the bootloader is configured for. This repo’s app build is configured to sign using `keys/proves.pem` (see `CONFIG_MCUBOOT_SIGNATURE_KEY_FILE` in `prj.conf`), so you must ensure that file matches the bootloader you flashed.

If you regenerate/replace the bootloader (or switch computers and flash a bootloader built elsewhere), make sure you also update `keys/proves.pem` to the matching signing key, or your built images will not boot.

You also want to make sure the authentication key the gds runs with is the same as the authentication key on the board. For that, you want to make sure the authentication key in FprimeZephyrReference/Components/Authenticate/AuthDefaultKey.h matches.

## Running Integration Tests

First, start GDS with:
```sh
make gds
```

Then, in another terminal, run the following command to execute the integration tests:
```sh
make test-integration
```

To run a single integration test file, set `TEST` to the filename (with or without `.py`):
```sh
make test-integration TEST=mode_manager_test
make test-integration TEST=mode_manager_test.py
```

### Testing for Flaky Tests

To debug intermittent integration test failures, use the interactive test runner to run tests multiple times:

```sh
# Interactive mode - select tests with arrow keys
make test-interactive

# Run specific tests multiple times
make test-interactive ARGS="--tests watchdog_test --cycles 10"

# Run all tests
make test-interactive ARGS="--all --cycles 20"
```

The runner automatically detects flaky tests and shows detailed statistics.

## Running The Radio With CircuitPython

To test the radio setup easily, you can use CircuitPython code on one board and fprime-zephyr on another. This provides a simple client/server setup and lets you observe what data is being sent through the radio.


On the board you want to receive data, make sure you have CircuitPython installed. Follow [these instructions](https://proveskit.github.io/pysquared/getting-started/). You can install the flight software or ground station code from that tutorial as these have the libraries you need, or simply install the CircuitPython firmware and manually add the required libraries.


Once you have CircuitPython running, upload the files from the ```circuit-python-lora-passthrough``` folder in this repo. Make sure to overwrite any existing ```boot.py``` and ```code.py``` files on your CircuitPython board with the ones from this folder.

> [!NOTE]
> If you're targeting a Feather rather than a flight controller board, then
> copy from ```circuit-python-lora-passthrough-feather``` instead.

```boot.py``` enables both virtual serial ports that the device presents over USB. This allows you to use one for the console and one for data. ```code.py``` acts as a LoRa radio forwarder over USB serial: The console port is used for logging and debugging, and is the first serial port that appears when the board is connected. The data port is used for actual data transfer, and is the second serial port.

1. Open the console port on your computer. This is the first serial port that opens when you plug in the circuitpython board. It should start by printing:

```
[INFO] LoRa Receiver receiving packets
[INFO] Packets received: 0
```

Once you have the board running the proves-core-reference radio code (make sure its plugged in!), you should start receiving packets and seeing this on the serial port

2. Now you want to be able to send commands through the radio. To do this, connect the gds to the circuitpython data port. Run the fprime-gds with the --uart-device parameter set to the serial port that is the second serial port that shows up when you plug in your circuitpython board

Depending on the comdelay, the gds should turn green every time a packet is sent. If you want to change this parameter use

```ReferenceDeployment.comDelay.DIVIDER_PRM_SET``` on the gds. You can set it down to 2, but setting it to 1 may cause issues.

## Sequences

You can control the specific command lists of the satellite by writing a sequence file. Sequence files are contained in /sequences. For details on how to attack the startup sequence check the sdd in Startup Manager.

## Conducting Over the Air Updates

When you run the gds,

``` fprime-gds --file-uplink-cooldown 0.8```

Now to fileuplink and update other parts. Upload Zephyr.signed.bin using the file uplink file


1. prepare image
2. update from (pass in the path)
3. configure_next_boot = test

to find the crc ./tools/bin/calculate-crc.py build-artifacts/zephyr.signed.bin

(either power cycle or run the reboot command, should reboot and come into that old version of software, check the version telemetry)

Go to components/flashworker

regionnumber = 1 try instead region number=2

(redo all the stuff)
