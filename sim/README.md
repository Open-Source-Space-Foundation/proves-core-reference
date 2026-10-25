# Software in the loop (SITL) on `native_sim`

The `native_sim` board builds the whole ReferenceDeployment as a Linux executable. The flight software boots, mounts its filesystem, runs its rate groups, and talks to a stock `fprime-gds` over a pseudo-terminal. You can run integration tests against it without a flight board.

What the board is made of:

- `boards/native_sim.overlay` gives every node label that `Main.cpp` and the topology use. Sensors sit on Zephyr's emulated I2C bus, GPIOs on `gpio-emul`, and the flash on a 16 MiB `sim-flash` file with the flight partition map. The SD card is a FAT flash disk on the storage partition.
- `boards/native_sim.conf` sets 10000 ticks per second like the flight boards. With native_sim's default of 100, every rate group runs 10 times too slow.
- `sim/` holds the fake devices and their bindings. `lora_sim.c` is a null radio. `rtc_sim.c` is Zephyr's `rtc_emul.c` changed to behave like the flight RV3028 (see its header). `ina219_sim.c` serves the two power monitors. `sensor_sim.c` serves the IMU (LSM6DSO and LIS2MDL) and the RP2350 die temperature sensor. `watchdog_sim.c` stands in for the external watchdog behind the JP6 jumper: it cold-reboots the sim when the Watchdog component stops toggling its pin for 44 s. `gpio_latch_sim.c` stands in for the MCP23017 that switches the face and payload power.

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

The build goes to `build-fprime-native_sim` and installs to `build-artifacts`, like the flight build. Whichever you built last is the one `make test-integration` and the GDS use.

## Run

The smoke run boots on a fresh flash image and checks `BootCount` across a power cut (`kill -9`) and a `WARM_RESET`. It takes about a minute.

```sh
sim/smoke.sh /tmp/sitl
```

To run the sim yourself, start it with a GDS attached, run tests, then stop it:

```sh
sim/run-sil.sh /tmp/sitl
make test-integration PYTEST_ARGS="--bare-flight-controler-board --with-watchdog-jumper"
sim/stop-sil.sh /tmp/sitl
```

`run-sil.sh` appends the sim's console to `sil.log` and the GDS output to `gds.log` in the run directory. The flash image `pcr-flash.bin` stays there between runs, so state persists like it does on a board. Delete it for a fresh filesystem. On a fresh image, `run-sil.sh` writes `0` to `Framing/src/sequence_number.bin` so the GDS starts at the flight software's sequence number.

`SIM_ARGS` passes options to the executable. `-rt-ratio=N` runs N times faster than real time, and `-no-rt` runs as fast as the host allows. Run `build-fprime-native_sim/zephyr/zephyr.exe --help` for the full list. Run the integration tests at real time, because their timeouts are in wall-clock time. At `-rt-ratio=10`, `rtc_test.py::test_05` sees its alarm fire early.

## Script the power monitors

The system INA219 reads 8.2 V and 0.3 A by default, and the solar INA219 reads 0 V and 0 A. To change a reading while the sim runs, write `<millivolts> [<milliamps>]` to `ina219_sys.txt` or `ina219_sol.txt` in the run directory. The fake reads the file on every sample, and keeps its last reading if the file is empty. Delete the file to go back to the default.

```sh
echo 6000 50 > /tmp/sitl/ina219_sys.txt   # ModeManager enters LOW_BATTERY safe mode about 10 s later
echo 8500 > /tmp/sitl/ina219_sys.txt      # and leaves it once the voltage recovers
```

## Script the IMU and die temperature

The LSM6DSO reads 1 g on Z, a gyro bias of about 0.1 °/s and 25 °C by default. The LIS2MDL reads 0.2 G on X and 0.4 G on Z, and the die sensor reads 25 °C. To change a reading while the sim runs, write one `<quantity> <value>...` line per quantity, in SI units, to `lsm6dso.txt`, `lis2mdl.txt` or `die_temp.txt` in the run directory. The quantities are `accel` and `gyro` (X Y Z), `magn` (X Y Z, in gauss) and `die-temp`. A quantity that the file leaves out reads its default.

```sh
printf 'gyro 0.5 0 0\n' > /tmp/sitl/lsm6dso.txt   # tumbling at 0.5 rad/s about X
```

## Known differences from flight

- The telemetry packetizer's divider sends most channels about every 5 minutes. Tests that wait for telemetry set `telemetryDelay.DIVIDER_PRM_SET 1` first.
- The solar INA219 reads 0 V and 0 A by default, because the sim has no sun.
- The IMU and die temperature fakes return the values above, not sensor noise. They accept any sampling rate and ignore it.
- The sensors behind the TCA9548A mux have no emulator, so they report "device not ready" or fail to read.
- The LoRa radio drops everything it sends and never receives.
- The external watchdog is always fitted, as if JP6 were bridged. It reboots the sim 44 s after the toggling stops, the time measured on a v5e bench board.
- No bootloader runs, so an OTA image written by `FlashWorker` is never swapped in, and `TRIGGER_BOOTLOADER` does nothing and never responds.
- When the sim exits at `-stop_at`, static destructors run and `CameraHandler`'s destructor crashes the process with exit code 139. Reboots and `kill` are not affected.
