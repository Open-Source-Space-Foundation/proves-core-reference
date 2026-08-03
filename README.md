# Proves Core Reference Project

This is a reference software implementation for the [PROVES Kit](https://docs.proveskit.space/en/latest/).

## Documentation

📚 **[Component Software Design Documents (SDDs)](https://open-source-space-foundation.github.io/proves-core-reference/)** - Browse detailed design documentation for all components.

## System Requirements
- F Prime System Requirements listed [here](https://fprime.jpl.nasa.gov/latest/docs/getting-started/installing-fprime/#system-requirements)
- Zephyr dependencies listed [here](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#install-dependencies) (NOTE: Only complete the install dependencies step, as we run through the rest of the steps in this readme.)
- [UV](https://docs.astral.sh/uv/getting-started/installation/) needs to be globally installed on your system.

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

Now you want to install the firmware onto the board. To do so, put the board into bootloader mode, then run
```shell
cp bootable.uf2 [path-to-your-board]
```
Run ```make build``` and reflash bootable.uf2 onto the board anytime you change code.

## Ground Station: F Prime GDS or YAMCS

You can use either the F Prime GDS or YAMCS as your mission control ground station.

### Option A: F Prime GDS (Traditional)

If this is your first time running the gds, you must create the authentication plug:
```shell
make framer-plugin
```

Finally, run the fprime-gds.
```shell
make gds
```

### Option B: YAMCS (Alternative Mission Control System)

[YAMCS](https://www.yamcs.org/) (Yet Another Mission Control System) is an alternative ground station interface that provides a web-based mission control system with real-time telemetry visualization, commanding, and parameter trending.

#### Setup and First Run

Before running YAMCS for the first time, generate the F Prime dictionary and set up the Python environment:

```shell
make fprime-venv
```

This creates the YAMCS configuration and applies necessary patches (packet preprocessor configuration, TM stream root container, and CPU fixes for the event processor).

Then start YAMCS with:

```shell
UART_DEVICE=/dev/ttyXXX make yamcs
```

YAMCS starts the following components:
1. **YAMCS Server** – web interface and mission control backbone (available at `http://localhost:8090`)
2. **F Prime Adapter** – communicates with the flight software over serial/TCP, translates telemetry frames (TM) and commands (TC)
3. **Events Bridge** – loads 655+ F Prime event definitions and streams events in real-time

#### Web Interface

Once YAMCS is running, open your browser to **`http://localhost:8090`** to see YAMCS running.

#### Stopping YAMCS

To cleanly shut down YAMCS and all its components:

```shell
make yamcs-stop
```

This kills the adapter, event bridge, and JVM, ensuring clean startup on the next `make yamcs` run.

#### Troubleshooting YAMCS

- **No parameters appearing:** Verify the `rootContainer` in `yamcs-data/mdb/fprime.xtce.xml` matches your deployment (e.g., `ReferenceDeployment`)
- **Port 8090 in use:** Run `make yamcs-stop` to ensure previous YAMCS processes are cleaned up
- **TM frame misalignment:** Caused by FSW console text on the serial UART. The adapter's rolling buffer and CRC validation handle this automatically
- **UnsupportedPacketVersionException warnings:** Cosmetic — idle fill bytes have invalid CCSDS version but valid packets are processed correctly

#### Ensuring your authentication/signing is correct

The Makefile will ensure the authentication is correct if you run the code on the same computer you flash on. However, if you switch from a computer that compiled the code you will likely have issues with authentication. Here are some things you may encounter

MCUBoot only boots images that are **signed with the same key** the bootloader is configured for. This repo’s app build is configured to sign using `keys/proves.pem` (see `CONFIG_MCUBOOT_SIGNATURE_KEY_FILE` in `prj.conf`), so you must ensure that file matches the bootloader you flashed.

If you regenerate/replace the bootloader (or switch computers and flash a bootloader built elsewhere), make sure you also update `keys/proves.pem` to the matching signing key, or your built images will not boot.

You also want to make sure the authentication key the gds runs with is the same as the authentication key provisioned on the board. The board's key lives in its on-flash key store (never in the image); ground reads its key from the `--authentication-key` CLI arg or the `PROVES_AUTH_KEY` env var. Make sure these match the key you provisioned with `PROVISION_KEY`/`ADD_KEY`.

##### Provisioning your first key

A freshly flashed board boots with an empty on-flash key store — no authentication key ever ships in the image. Because the store is empty, the board allows exactly one unauthenticated command: `PROVISION_KEY`. Once any key is provisioned, `PROVISION_KEY` is refused, so this only works the first time (or again after every key has been removed).

1. Start GDS as normal (`make gds`).
2. From the GDS command view, send `PROVISION_KEY(spi=<n>, key=<32 hex chars>)`, e.g. `PROVISION_KEY(0, 00112233445566778899aabbccddeeff)`.
3. Tell ground to use the same key for every command after that: set `PROVES_AUTH_KEY` to the same hex string (or pass `--authentication-key`). If you provisioned a non-zero SPI, also run gds with `make gds SPI=<n>` (or pass `--spi <n>` directly) so ground's outgoing frames carry the matching SPI.

The board holds up to two active keys so a rotation never leaves you locked out. Ground uses exactly one key at a time (whichever `--authentication-key`/`PROVES_AUTH_KEY` it was started with), so rotate in this order:

1. Keep running GDS with the **old** key and send `ADD_KEY(new_spi, new_key)` — the command itself has to authenticate under the old key.
2. Restart GDS with the **new** key (and `--spi new_spi`), and confirm commands are accepted.
3. Only then send `REMOVE_KEY(old_spi)`, authenticated under the new key.

Doing step 3 before step 2 works too, but leaves nothing to fall back on if the new key turns out to be wrong. `REMOVE_KEY` refuses to remove the last remaining key.

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

Updates are performed by MCUboot in swap mode. Flash is divided into a bootloader
partition, a **primary slot** (`slot0_partition`, where the running firmware
lives) and a **secondary slot** (`slot1_partition`, the staging area). You uplink
a new signed image to the on-board filesystem, copy it into the secondary slot,
mark it for boot, and reboot — MCUboot swaps the two slots and runs the new
image. If the new image is booted in `TEST` mode and never confirmed, the next
reboot swaps back.

Both slots are 1 MB (see
`boards/bronco_space/proves_flight_control_board_v5/proves_flight_control_board_v5.dtsi`),
so an image must fit in 1 MB minus the MCUboot trailer.

### Prerequisites

- The board is running the MCUboot bootloader (see [Bootloader (MCUBoot)](#bootloader-mcuboot)).
- `keys/proves.pem` is the key the installed bootloader was built with. An image
  signed with a different key will be rejected by MCUboot and the board will
  fall back to the old image.
- A signed image to install: `make build` writes `build-artifacts/zephyr.signed.bin`.

### Procedure

Start the GDS with a file-uplink cooldown, since the image is large:

```shell
fprime-gds --file-uplink-cooldown 0.8
```

1. **Compute the image CRC.** Flight software verifies it before writing a
   single byte, so this must be the value the flight-side CRC produces:

   ```shell
   ./tools/bin/calculate-crc.py build-artifacts/zephyr.signed.bin
   ```

2. **Uplink the image** to the satellite filesystem with the GDS file uplink
   panel, e.g. to `/update/zephyr.signed.bin`.

3. **`Update.updater.PREPARE_UPDATE`** — erases the secondary slot. Wait for
   `PrepareUpdateSucceeded`.

4. **`Update.updater.UPDATE_IMAGE_FROM`** with the uplinked path and the CRC
   from step 1. Wait for `UpdateSucceeded`. A CRC mismatch surfaces as
   `Update.worker.ImageFileCrcMismatch`.

5. **`Update.updater.CONFIGURE_NEXT_BOOT`** with `TEST`. Use `TEST`, not
   `PERMANENT`: a `TEST` image that fails to boot is automatically reverted,
   a `PERMANENT` one is not.

6. **Reboot** — `ReferenceDeployment.resetManager.COLD_RESET`, or power cycle.
   MCUboot performs the swap during boot, which takes noticeably longer than a
   normal reset.

7. **Verify** the new image is running: send `CdhCore.version.VERSION` with
   `PROJECT` and check the `ProjectVersion` event against the version of the
   build you uplinked.

8. **`Update.updater.CONFIRM_UPDATE`** — makes the swap permanent. Until you do
   this, the *next* reboot reverts to the previous image.

### Testing it

`PROVESFlightControllerReference/test/int/ota_test.py` runs this whole cycle
against real hardware and checks the reported project version after the swap.
It is excluded from the default integration run because it erases a flash slot,
uplinks a large file and reboots the board:

```shell
make test-integration TEST=ota_test.py FILTER=ota
```

Run against the build in your working tree, it uplinks the image the board is
already running, so the post-swap version check passes trivially. To make the
check meaningful, point `--ota-image` at a *different* build:

```shell
make test-integration TEST=ota_test.py FILTER=ota \
  PYTEST_ARGS="--ota-image=/path/to/other/zephyr.signed.bin --ota-expect-version=v1.2.3"
```

In CI this is the `integration-ota` job. It is not part of the normal PR run —
it ties up the integration cube for the better part of an hour. Trigger it by
running the `ci` workflow manually with **Run the hardware over-the-air update
test** checked, or by adding the `test-ota` label to a PR. The job builds a
second image under a throwaway git tag so the project version differs from the
one flashed on the board, which is what lets it prove a swap actually happened.

Uplink dominates the runtime: at the `fprime-gds.yml` defaults
(`file-uplink-chunk-size: 204`, `file-uplink-cooldown: 0.400`) a 1.4 MB image is
about 7100 chunks, ~48 minutes. Lower the cooldown to go faster — locally via
`make gds-integration GDS_EXTRA_ARGS="--file-uplink-cooldown 0.05"`, or in CI via
the workflow's uplink-cooldown input.

### If an update goes wrong

- **The board stops responding right after `PREPARE_UPDATE`.** That means the
  erase hit the running slot instead of the staging slot. `FlashWorker` resolves
  the target from the `slot1_partition` devicetree label
  (`PARTITION_ID(slot1_partition)`); do **not** replace this with a literal
  number. Zephyr assigns flash-area IDs in devicetree dependency-ordinal order,
  so adding any partition anywhere in the devicetree renumbers every area, and a
  hardcoded ID starts pointing at a different partition. Recover by copying
  `bootable.uf2` onto the board in UF2 bootloader mode, or over SWD with
  `make debug-install bootable.signed.hex`.
- **The board boots the old image after the swap reboot.** MCUboot rejected the
  staged image — usually a signature mismatch (`keys/proves.pem` does not match
  the installed bootloader) or an image that overflows the slot.
- **The new image works, then disappears after a later reboot.** `CONFIRM_UPDATE`
  was never sent, so MCUboot reverted the trial boot.
