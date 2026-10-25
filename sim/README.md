# Software in the loop (SITL) on `native_sim`

The `native_sim` board builds the whole ReferenceDeployment as a Linux executable. The flight software boots, mounts its filesystem, runs its rate groups, and talks to a stock `fprime-gds` over a pseudo-terminal. You can run integration tests against it without a flight board.

What the board is made of:

- `boards/native_sim.overlay` gives every node label that `Main.cpp` and the topology use. Sensors sit on Zephyr's emulated I2C bus, GPIOs on `gpio-emul`, and the flash on a 16 MiB `sim-flash` file with the flight partition map. The SD card is a FAT flash disk on the storage partition.
- `boards/native_sim.conf` sets 10000 ticks per second like the flight boards. With native_sim's default of 100, every rate group runs 10 times too slow.
- `sim/` holds the fake devices and their bindings. `lora_sim.c` is a null radio. `rtc_sim.c` is Zephyr's `rtc_emul.c` changed to behave like the flight RV3028 (see its header). `sensor_sim.c` serves the IMU (LSM6DSO and LIS2MDL), the RP2350 die temperature sensor and the two INA219 power monitors. `watchdog_sim.c` stands in for the external watchdog behind the JP6 jumper: it cold-reboots the sim when the Watchdog component stops toggling its pin for 44 s. `gpio_latch_sim.c` stands in for the MCP23017 that switches the face and payload power.
- `sim/` is a Zephyr module, which the root `CMakeLists.txt` adds only for native_sim boards. Each fake has a Kconfig symbol in `sim/Kconfig` that turns on when the overlay has a node with its compatible, and a source line in `sim/CMakeLists.txt`.

Devices with no emulator or fake report "device not ready", and the flight software handles that.

## Set up

You need a Linux host. You do not need the Zephyr SDK, because native_sim builds with the host `gcc`.

On x86-64, `make sim-build` builds the 32-bit `native_sim`, which needs the 32-bit toolchain:

```sh
sudo apt-get install -y gcc-multilib g++-multilib
make sim-setup
```

On any other host, such as arm64 Linux, `make sim-build` builds `native_sim/native/64`, because only x86 can run 32-bit host code. `boards/native_sim_native_64.*` link to the `native_sim` files, and `PROVESFlightControllerReference/project/config/CMakeLists.txt` doubles every F Prime OS handle size when `CONFIG_64BIT` is set, because 64-bit pointers overflow the sizes trimmed for the RP2350. Flight builds use `PlatformCfg.fpp` unchanged. Set `SIM_BOARD` to pick the board yourself.

The 64-bit build differs from flight in one more way than the 32-bit build. `long` and pointers are 8 bytes, so it cannot catch code that assumes 4. Command, event, and telemetry formats are fixed-width and do not change. CI runs the 32-bit build.

### Apple Silicon

Run the SITL in an arm64 Ubuntu VM with [Lima](https://lima-vm.io). It needs no `sudo` on the Mac.

```sh
brew install lima
limactl start --name=pcr-sim sim/lima.yaml
limactl shell pcr-sim
# in the VM
git clone https://github.com/Open-Source-Space-Foundation/proves-core-reference.git && cd proves-core-reference
make sim-setup
```

Then build and run in the VM as below. `limactl stop pcr-sim` stops the VM.

## Build

```sh
make sim-build
```

The build goes to `build-fprime-native_sim` and installs to `build-artifacts-sim`, so it never overwrites the flight build in `build-artifacts`. Later runs of `make sim-build` reuse the build directory. Delete it to regenerate. Rebuild with `make sim-build`, because `fprime-util build` rejects a build cache whose install path differs from `settings.ini`.

## Run

The smoke run boots on a fresh flash image and checks `BootCount` across a power cut (`kill -9`) and a `WARM_RESET`. It takes about a minute.

```sh
sim/smoke.sh /tmp/sitl
```

To run the sim yourself, start it with a GDS attached, run tests, then stop it:

```sh
sim/run-sil.sh /tmp/sitl
make sim-test SIM_RUN=/tmp/sitl PYTEST_ARGS="--bare-flight-controler-board --with-watchdog-jumper"
sim/stop-sil.sh /tmp/sitl
```

`run-sil.sh` appends the sim's console to `sil.log` and the GDS output to `gds.log` in the run directory. The flash image `pcr-flash.bin` stays there between runs, so state persists like it does on a board. Delete it for a fresh filesystem. The run directory also holds the sim's own `sequence_number.bin`, so the sim never touches the board's `Framing/src/sequence_number.bin`. On a fresh image, `run-sil.sh` writes `0` to it so the GDS starts at the flight software's sequence number. `make sim-test` points the tests at the same file. `stop-sil.sh` exits 1 if the sim or the GDS is still running after it stops them.

`SIM_ARGS` passes options to the executable. `-rt-ratio=N` runs N times faster than real time, and `-no-rt` runs as fast as the host allows. Run `build-fprime-native_sim/zephyr/zephyr.exe --help` for the full list. Run the integration tests at real time, because their timeouts are in wall-clock time. At `-rt-ratio=10`, `rtc_test.py::test_05` sees its alarm fire early.

## Script the sensors

Every fake sensor reads a file in the run directory on each sample, so you can change a reading while the sim runs. Write one `<quantity> <value>...` line per quantity, in SI units.

| File | Quantities | Default |
| --- | --- | --- |
| `ina219_sys.txt` | `voltage` (V), `current` (A) | 8.2 V, 0.3 A |
| `ina219_sol.txt` | `voltage` (V), `current` (A) | 0 V, 0 A |
| `lsm6dso.txt` | `accel` (m/s², X Y Z), `gyro` (rad/s, X Y Z), `die-temp` (°C) | 1 g on Z, about 0.1 °/s gyro bias, 25 °C |
| `lis2mdl.txt` | `magn` (gauss, X Y Z) | 0.2 G on X, 0.4 G on Z |
| `die_temp.txt` | `die-temp` (°C) | 25 °C |

The INA219s report power as voltage times current. A quantity that the file leaves out reads its default, and deleting the file returns every quantity to its default. An empty file keeps the last reading. A line with a missing, non-numeric or out-of-range value leaves that quantity at its default, never half updated.

```sh
echo 'voltage 6.0' > /tmp/sitl/ina219_sys.txt    # ModeManager enters LOW_BATTERY safe mode about 10 s later
echo 'voltage 8.5' > /tmp/sitl/ina219_sys.txt    # and leaves it once the voltage recovers
printf 'gyro 0.5 0 0\n' > /tmp/sitl/lsm6dso.txt   # tumbling at 0.5 rad/s about X
```

A shell redirect truncates the file before it writes, so a sample can land in between. To change several quantities at once without that window, write a temporary file and rename it over the real one, as in `printf 'voltage 6.0\ncurrent 0.05\n' > /tmp/sitl/.ina219_sys && mv /tmp/sitl/.ina219_sys /tmp/sitl/ina219_sys.txt`.

## Known differences from flight

- The telemetry packetizer's divider sends most channels about every 5 minutes. Tests that wait for telemetry set `telemetryDelay.DIVIDER_PRM_SET 1` first.
- The solar INA219 reads 0 V and 0 A by default, because the sim has no sun.
- The sensor fakes return the values above, not sensor noise. They accept any sampling rate and ignore it.
- The sensors behind the TCA9548A mux have no emulator, so they report "device not ready" or fail to read.
- The LoRa radio drops everything it sends and never receives.
- The external watchdog is always fitted, as if JP6 were bridged. It reboots the sim 44 s after the toggling stops, the time measured on a v5e bench board.
- No bootloader runs, so an OTA image written by `FlashWorker` is never swapped in, and `TRIGGER_BOOTLOADER` does nothing and never responds.
- When the sim exits at `-stop_at`, static destructors run and `CameraHandler`'s destructor crashes the process with exit code 139. Reboots and `kill` are not affected.
