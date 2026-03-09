# Proves Core Reference Project - Copilot Instructions

## Project Overview

This is a reference software implementation for the [Proves Kit](https://docs.proveskit.space/en/latest/), combining F Prime (NASA's flight software framework) with Zephyr RTOS to create firmware for embedded flight control boards. The project targets ARM Cortex-M microcontrollers, specifically RP2350 (Raspberry Pi Pico 2) and STM32 boards.

**Repository Size**: ~450MB (primarily from Zephyr workspace and F Prime submodules)
**Languages**: C++ (F Prime components), C (Zephyr integration), Python (testing), FPP (F Prime modeling language), CMake
**Frameworks**: F Prime v4.0.0a1, Zephyr RTOS v4.2.0
**Target Runtime**: Bare metal embedded systems (ARM Cortex-M33/M7)

## Critical Build Prerequisites

### System Dependencies

Before any build steps, ensure you have:

- Python 3.13+ (specified in `.python-version`)
- F Prime system requirements: https://fprime.jpl.nasa.gov/latest/docs/getting-started/installing-fprime/#system-requirements
- Zephyr dependencies: https://docs.zephyrproject.org/latest/develop/getting_started/index.html#install-dependencies
  - **NOTE**: Only install dependencies; do NOT run Zephyr's full setup (handled by Makefile)

### Build Tool: UV Package Manager

This project uses **UV** (v0.8.13) for Python environment management. It is automatically downloaded by the Makefile.

- Do NOT use `pip` or `python -m venv` directly
- Always use `make` targets which invoke UV internally

## Build & Test Workflow

### First-Time Setup (Complete Sequence)

Run these commands **in order** from the repository root. The entire setup takes ~5-10 minutes:

```bash
# Step 1: Initialize git submodules (~1 minute)
make submodules

# Step 2: Create Python virtual environment and install dependencies (~2 minutes)
make fprime-venv

# Step 3: Set up Zephyr workspace and SDK (~3-5 minutes)
make zephyr

# Step 4: Generate F Prime build cache (~1 minute)
make generate

# Step 5: Build the firmware (~2-3 minutes)
make build
```

**IMPORTANT**: The `make` command (default target) runs all of the above automatically.

**Alternative Zephyr Setup**: The new Makefile structure provides more granular Zephyr control:

```bash
# Complete Zephyr setup (equivalent to zephyr)
make zephyr

# Or step-by-step Zephyr setup
make zephyr-config     # Configure west
make zephyr-workspace  # Setup modules and tools
make zephyr-export     # Export environment
make zephyr-python-deps # Install Python dependencies
make zephyr-sdk        # Install SDK
```

### Development Workflow

**After making code changes**:

```bash
# Always run build (it calls generate-if-needed automatically)
make build
```

**When to run generate explicitly**:

- After modifying `.fpp` files (F Prime component definitions)
- After changing CMakeLists.txt files
- After modifying core F Prime package files
- Command: `make generate`

### Linting & Formatting

```bash
# Run all pre-commit checks (REQUIRED before committing)
make fmt
```

This runs:

- `clang-format` (C/C++ formatting)
- `cpplint` (C++ style checking using `cpplint.cfg`)
- `ruff` (Python linting and formatting)
- `codespell` (spell checking)
- Standard pre-commit hooks (trailing whitespace, JSON/YAML validation, etc.)

**Configuration**: `.pre-commit-config.yaml`, `cpplint.cfg`, `.clang-format`

### Testing Framework

**Unit Tests**:

```bash
make test-unit
```

Uses CMake/CTest. Unit tests are in `PROVESFlightControllerReference/test/unit-tests/`.

**Interactive Test Selection**:

```bash
make test-interactive          # Launch interactive test menu
make test-interactive ARGS="--all --cycles 10"  # CLI mode
```

**Integration Test Workflow**:
Integration tests require a two-terminal setup with the GDS (Ground Data System) running:

```bash
# Terminal 1: Start GDS (Ground Data System)
make gds
# This starts fprime-gds with:
# - Dictionary: build-artifacts/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json
# - Communication: UART at 115200 baud
# - Output: Unframed data mode

# Terminal 2: Run integration tests
make test-integration
# This runs: pytest PROVESFlightControllerReference/test/int --deployment build-artifacts/zephyr/fprime-zephyr-deployment
```

**Test Framework Details**:

- **Location**: `PROVESFlightControllerReference/test/int/`
- **Framework**: pytest with fprime-gds testing API
- **Test Files**:
  - `watchdog_test.py` - Watchdog component integration tests
  - `imu_manager_test.py` - IMU Manager component tests
  - `rtc_test.py` - RTC Manager component tests
- **API**: Uses `IntegrationTestAPI` for sending commands and asserting events/telemetry
- **Communication**: Tests communicate with the board via GDS over UART

**Test Prerequisites**:

1. Board must be connected via USB (appears as CDC ACM device)
2. Firmware must be flashed and running
3. GDS must be running and connected to the board
4. Board must be in operational state (not in bootloader mode)

**Test Examples**:

- **Watchdog Tests**: Start/stop watchdog, verify transition counting
- **IMU Tests**: Send telemetry packets, verify acceleration data
- **RTC Tests**: Set/get time, verify time synchronization

## Project Structure

### Repository Root Files

```
CMakeLists.txt          # Top-level CMake configuration
CMakePresets.json       # CMake presets for Zephyr build
Makefile               # Primary build interface
settings.ini           # F Prime project settings (default board, toolchain)
west.yml               # Zephyr workspace manifest
Kconfig                # Zephyr configuration options
prj.conf               # Zephyr project configuration (USB, I2C, SPI, etc.)
fprime-gds.yml         # GDS command-line defaults
requirements.txt       # Python dependencies
.python-version        # Python version requirement (3.13)
```

### Directory Structure

```
PROVESFlightControllerReference/
├── Components/        # Custom F Prime components
│   ├── ADCS/          # Attitude Determination and Control System
│   ├── AmateurRadio/  # Amateur radio communication
│   ├── AntennaDeployer/ # Antenna deployment mechanism
│   ├── Authenticate/  # HMAC-based command authentication
│   ├── AuthenticationRouter/ # Authentication routing
│   ├── BootloaderTrigger/ # Bootloader mode entry
│   ├── Burnwire/      # Burnwire deployment mechanism
│   ├── CameraHandler/ # Camera management
│   ├── ComDelay/      # Communication delay management
│   ├── DetumbleManager/ # Magnetic detumble control
│   ├── Drv/           # Driver components (IMU, RTC, sensor managers)
│   │   ├── Drv2605Manager/ # Haptic driver
│   │   ├── Ina219Manager/  # Current/power monitor driver
│   │   ├── RtcManager/     # RTC driver
│   │   ├── Tmp112Manager/  # Temperature sensor driver
│   │   └── Veml6031Manager/ # Light sensor driver
│   ├── FatalHandler/  # System error handling
│   ├── FlashWorker/   # Flash memory management
│   ├── FsFormat/      # Filesystem formatting
│   ├── FsSpace/       # Filesystem space monitoring
│   ├── ImuManager/    # LSM6DSO 6-axis IMU management
│   ├── LoadSwitch/    # Power load switch control
│   ├── ModeManager/   # Spacecraft mode management
│   ├── NullPrmDb/     # No-op parameter database
│   ├── PayloadCom/    # Payload communication
│   ├── PowerMonitor/  # Power system monitoring
│   ├── ResetManager/  # System reset management
│   ├── SBand/         # S-Band radio communication
│   ├── StartupManager/ # Startup sequence management
│   ├── ThermalManager/ # Thermal monitoring and control
│   └── Watchdog/      # Hardware watchdog with LED indication
├── ComCcsdsLora/      # CCSDS LoRa communication framing
├── ComCcsdsSband/     # CCSDS S-Band communication framing
├── ComCcsdsUart/      # CCSDS UART communication framing
├── ReferenceDeployment/
│   ├── Main.cpp      # Application entry point
│   └── Top/          # Topology definition
│       ├── instances.fpp   # Component instances
│       └── topology.fpp    # Component connections
├── project/
│   └── config/       # Project-wide FPP configuration
└── test/
    └── int/          # Integration tests (pytest)

lib/
├── fprime/           # F Prime framework (39MB, git submodule)
├── fprime-zephyr/    # F Prime-Zephyr integration (368KB, git submodule)
├── fprime-extras/    # Additional F Prime libraries
└── zephyr-workspace/ # Zephyr RTOS (404MB, git submodule)

boards/               # Custom board definitions
└── bronco_space/
    ├── proves_flight_control_board_v5/     # Base board definition
    ├── proves_flight_control_board_v5c/    # Variant C (LED on GPIO 24)
    └── proves_flight_control_board_v5d/    # Variant D (standard configuration)

bootloader/           # MCUBoot bootloader configuration
Framing/              # F Prime framing/deframing plugin (pip installable)
sequences/            # F Prime command sequences (.seq files)
scripts/              # Utility scripts (key generation, etc.)
tools/                # Development and analysis tools (see tools/README.md)
docs-site/
├── getting-started/     # Setup and build documentation
├── additional-resources/  # Board-specific guides, troubleshooting
├── uploading/           # Board-specific firmware upload instructions
└── components/          # Component SDD documentation
```

### Key Architecture Points

**F Prime + Zephyr Integration**:

- F Prime components defined in `.fpp` files (autocoded to C++)
- Zephyr handles RTOS, drivers, and hardware abstraction
- Main entry point: `PROVESFlightControllerReference/ReferenceDeployment/Main.cpp`
  - **Critical**: 3-second sleep before starting to allow USB CDC ACM initialization
- Build system: CMake with F Prime and Zephyr toolchains
- Default board: `proves_flight_control_board_v5d/rp2350a/m33` (configurable in `settings.ini`)

## Board Variations & Hardware Configuration

### Available Board Versions

The project supports multiple variants of the PROVES Flight Control Board, all based on the RP2350 (Raspberry Pi Pico 2) microcontroller:

**Base Board (`proves_flight_control_board_v5`)**:

- Common hardware definition shared by all variants
- Defines sensors: LSM6DSO (IMU), LIS2MDL (magnetometer), INA219 (current sensor)
- LoRa radio: SX1276 with SPI interface
- RTC: RV3028 with I2C interface
- USB CDC ACM for console communication
- Watchdog LED on GPIO 23 (base configuration)

**Variant C (`proves_flight_control_board_v5c`)**:

- **Key Difference**: Watchdog LED moved to GPIO 24
- LoRa DIO pins: GPIO 13 and GPIO 12 (different from base)
- USB Product ID: "PROVES Flight Control Board v5c"
- As we develop, probably other differences will be noticed

**Variant D (`proves_flight_control_board_v5d`)**:

- **Key Difference**: Uses base board configuration (LED on GPIO 23)
- LoRa DIO pins: GPIO 14 and GPIO 13 (base configuration)
- USB Product ID: "PROVES Flight Control Board v5d"
- Most similar to the original v5 design
- **Default Board**: This is the default in `settings.ini`

### Board Selection

- **Default**: Set in `settings.ini` (`BOARD=proves_flight_control_board_v5d/rp2350a/m33`)
- **Override**: Use CMake option `-DBOARD=<board_name>/<soc>/<core>`
- **Available SOCs**: `rp2350a` (Raspberry Pi Pico 2)
- **Available Cores**: `m33` (ARM Cortex-M33)

**Component Types**:

- `.fpp` files: F Prime component definitions (autocoded)
- `.cpp/.hpp` files: Implementation code
- `CMakeLists.txt` in each component: Build registration

## Build System Details

### Generated Artifacts Location

```
build-fprime-automatic-zephyr/  # F Prime + Zephyr build cache
build-artifacts/
├── zephyr.uf2                 # Firmware binary for flashing
└── zephyr/fprime-zephyr-deployment/
    └── dict/ReferenceDeploymentTopologyDictionary.json  # For GDS
```

### CMake Configuration

- **Toolchain**: `lib/fprime-zephyr/cmake/toolchain/zephyr.cmake`
- **Build Type**: Release
- **Board Root**: Repository root (custom board definitions in `boards/`)
- **Important Options**:
  - `FPRIME_ENABLE_FRAMEWORK_UTS=OFF` (no framework unit tests)
  - `FPRIME_ENABLE_AUTOCODER_UTS=OFF` (no autocoder unit tests)

### Makefile Targets Reference

```bash
make help              # Show all available targets
make submodules        # Initialize git submodules
make fprime-venv       # Create Python virtual environment
make zephyr            # Set up Zephyr workspace and ARM toolchain (from lib/makelib/zephyr.mk)
make zephyr-setup      # Conditional Zephyr setup (skips if already set up)
make generate          # Generate F Prime build cache (force)
make generate-if-needed # Generate only if build directory missing
make build             # Build firmware (runs generate-if-needed)
make build-mcuboot     # Build firmware with MCUBoot bootloader signing
make generate-auth-key # Generate AuthDefaultKey.h with a random HMAC key
make fmt               # Run linters and formatters (pre-commit)
make data-budget       # Analyze telemetry data budget (use VERBOSE=1 for details)
make docs-sync         # Sync component SDD files to docs-site/components/
make docs-serve        # Serve MkDocs documentation site locally (http://127.0.0.1:8000)
make docs-build        # Build MkDocs documentation site
make gds               # Start F Prime Ground Data System
make gds-integration   # Start GDS without GUI (for CI)
make framer-plugin     # Build and install the CCSDS framing plugin
make sequence SEQ=<name> # Compile a sequence file from sequences/ directory
make sync-sequence-number # Synchronize GDS/flight sequence number
make test-unit         # Run unit tests (CMake/CTest based)
make test-integration  # Run integration tests (requires connected board)
make test-interactive  # Run interactive test selection (use ARGS= for CLI mode)
make bootloader        # Trigger bootloader mode on RP2350
make copy-secrets SECRETS_DIR=<dir> # Copy signing keys and auth key to repo
make make-ci-spacecraft-id # Generate unique spacecraft ID for CI builds
make clean             # Remove all gitignored files
make uv                # Download UV package manager
make pre-commit-install # Install pre-commit hooks
make minimize-uv-cache # Minimize UV cache (CI optimization)
```

**Zephyr-Specific Targets** (from `lib/makelib/zephyr.mk`):

```bash
make zephyr            # Complete Zephyr setup (config + workspace + export + deps + SDK)
make zephyr-config     # Configure west
make zephyr-workspace  # Setup Zephyr bootloader, modules, and tools
make zephyr-export     # Export Zephyr environment variables
make zephyr-python-deps # Install Zephyr Python dependencies
make zephyr-sdk        # Install Zephyr SDK
make clean-zephyr      # Remove all Zephyr build files
make clean-zephyr-config    # Remove west configuration
make clean-zephyr-workspace # Remove Zephyr bootloader, modules, and tools
make clean-zephyr-export    # Remove Zephyr exported files
make clean-zephyr-sdk      # Remove Zephyr SDK
```

### CI/CD Pipeline (`.github/workflows/ci.yaml`)

**Jobs**:

1. **Lint**: Runs `make fmt` (pre-commit checks)
2. **Build**: Full build with caching
   - Caches: bin tools, submodules, Python venv, Zephyr workspace
   - Runs: `make submodules`, `make fprime-venv`, `make zephyr`, `make generate-ci build-ci`
   - Uploads: `build-artifacts/zephyr.uf2` and dictionary JSON

**Critical for CI Success**:

- Always run `make fmt` before pushing
- Ensure code builds with `make build` locally
- Integration tests are NOT run in CI (require hardware)

## Common Issues & Workarounds

### Issue: Build Fails with "west not found"

**Solution**: Run `make zephyr` to install west and Zephyr SDK.

### Issue: "No such file or directory: fprime-util"

**Solution**: Run `make fprime-venv` to create virtual environment with dependencies.

### Issue: CMake cache errors after changing board configuration

**Solution**: Run `make clean` followed by `make generate build`.

### Issue: USB device not detected on board

**Workaround**: The board may need to be put into bootloader mode. Use the new bootloader target:

```bash
make bootloader
```

This automatically detects if the board is already in bootloader mode and triggers it if needed. See board-specific guides in `docs-site/additional-resources/board-list.md`.

### Issue: Integration tests fail to connect

**Solution**: Ensure GDS is running (`make gds`) and board is connected. Check serial port in GDS output.

### Issue: Build times out (>2 minutes)

**Solution**: First build takes 3-5 minutes. Subsequent builds are faster (~30 seconds). Use `timeout: 300` for initial builds.

### Issue: Flashing firmware to board

**Different boards require different methods**:

- **RP2040/RP2350**: Copy `.uf2` file to board's USB mass storage
  ```bash
  cp build-artifacts/zephyr.uf2 /Volumes/RPI-RP2  # macOS
  ```
- **STM32**: Use STM32CubeProgrammer via SWD
  ```bash
  sh ~/Library/Arduino15/packages/STMicroelectronics/tools/STM32Tools/2.3.0/stm32CubeProg.sh \
    -i swd -f build-artifacts/zephyr/zephyr.hex -c /dev/cu.usbmodem142203
  ```
- See `docs-site/additional-resources/board-list.md` for tested boards

## File Modification Guidelines

### When modifying F Prime components:

1. Edit `.fpp` files for interface changes (ports, commands, telemetry, events)
2. Edit `.cpp/.hpp` files for implementation
3. Run `make generate` to regenerate autocoded files
4. Run `make build` to compile
5. Run `make fmt` to lint

### Understanding ReferenceDeploymentTopology and DT_NODE

**Topology Architecture**:
The `ReferenceDeploymentTopology` is the central coordination point for the F Prime application:

**Key Files**:

- `PROVESFlightControllerReference/ReferenceDeployment/Top/ReferenceDeploymentTopology.cpp` - Main topology implementation
- `PROVESFlightControllerReference/ReferenceDeployment/Top/topology.fpp` - FPP topology definition
- `PROVESFlightControllerReference/ReferenceDeployment/Top/instances.fpp` - Component instances

**DT_NODE Usage**:
The topology uses Zephyr Device Tree nodes to access hardware:

```cpp
// Example from ReferenceDeploymentTopology.cpp
static const struct gpio_dt_spec ledGpio = GPIO_DT_SPEC_GET(DT_NODELABEL(led0), gpios);
```

**DT_NODE Explanation**:

- `DT_NODELABEL(led0)` - References the `led0` node from device tree
- `GPIO_DT_SPEC_GET()` - Extracts GPIO specification (port, pin, flags)
- Device tree defines hardware mapping: `led0: led0 { gpios = <&gpio0 23 GPIO_ACTIVE_HIGH>; }`
- Board variants override these mappings (e.g., v5c uses GPIO 24, v5d uses GPIO 23)

**Topology Initialization Sequence**:

1. `initComponents()` - Initialize all F Prime components
2. `setBaseIds()` - Set component ID offsets
3. `connectComponents()` - Wire component ports together
4. `regCommands()` - Register command handlers
5. `configComponents()` - Configure component parameters
6. `loadParameters()` - Load initial parameter values
7. `startTasks()` - Start active component tasks

**Hardware Integration**:

- Topology bridges F Prime components with Zephyr device drivers
- Uses device tree nodes to access sensors, GPIO, UART, etc.
- Board-specific configurations handled through device tree overlays

### When modifying topology:

1. Edit `PROVESFlightControllerReference/ReferenceDeployment/Top/instances.fpp` for new component instances
2. Edit `PROVESFlightControllerReference/ReferenceDeployment/Top/topology.fpp` for connections
3. Run `make generate build`

### When adding new components:

1. Create component directory under `PROVESFlightControllerReference/Components/`
2. Add `CMakeLists.txt` with `register_fprime_library()` or `register_fprime_module()`
3. Add component to parent `CMakeLists.txt` with `add_fprime_subdirectory()`
4. Follow existing component structure (see `Components/Watchdog/` as example)

### ReferenceDeployment Topology and DT_NODE Integration

**Understanding the Topology System**:
The `ReferenceDeploymentTopology` serves as the central coordinator that bridges F Prime components with Zephyr hardware drivers through Device Tree nodes.

**Key Topology Files**:

- `PROVESFlightControllerReference/ReferenceDeployment/Top/ReferenceDeploymentTopology.cpp` - Main implementation
- `PROVESFlightControllerReference/ReferenceDeployment/Top/topology.fpp` - FPP topology definition
- `PROVESFlightControllerReference/ReferenceDeployment/Top/instances.fpp` - Component instances
- `PROVESFlightControllerReference/ReferenceDeployment/Top/ReferenceDeploymentTopologyDefs.hpp` - Type definitions

**DT_NODE Usage Pattern**:

```cpp
// Example from ReferenceDeploymentTopology.cpp
static const struct gpio_dt_spec ledGpio = GPIO_DT_SPEC_GET(DT_NODELABEL(led0), gpios);
```

**How DT_NODE Works**:

1. **Device Tree Definition**: Hardware is defined in `.dts` files (e.g., `led0: led0 { gpios = <&gpio0 23 GPIO_ACTIVE_HIGH>; }`)
2. **DT_NODELABEL**: References the device tree node by label (`led0`)
3. **GPIO_DT_SPEC_GET**: Extracts GPIO specification (port, pin, flags) at compile time
4. **Board Variants**: Different boards override these mappings (v5c uses GPIO 24, v5d uses GPIO 23)

**Topology Initialization Sequence**:

```cpp
void setupTopology(const TopologyState& state) {
    initComponents(state);      // 1. Initialize F Prime components
    setBaseIds();              // 2. Set component ID offsets
    connectComponents();        // 3. Wire component ports together
    regCommands();             // 4. Register command handlers
    configComponents(state);   // 5. Configure component parameters
    loadParameters();          // 6. Load initial parameter values
    startTasks(state);         // 7. Start active component tasks
}
```

**Hardware Integration Points**:

- **GPIO Access**: `GPIO_DT_SPEC_GET(DT_NODELABEL(led0), gpios)` for LED control
- **UART Communication**: Device tree defines CDC ACM UART for console/GDS
- **Sensor Access**: I2C/SPI devices defined in device tree (LSM6DSO, LIS2MDL, etc.)
- **Board-Specific Overrides**: Each board variant can override device tree nodes

**Adding Hardware-Accessing Components**:
When creating components that need hardware access:

1. Define device tree nodes in board `.dts` files
2. Use `DT_NODELABEL()` and `GPIO_DT_SPEC_GET()` in component code
3. Add component to topology in `instances.fpp` and `topology.fpp`
4. Configure hardware access in `ReferenceDeploymentTopology.cpp`

### When modifying board configuration:

1. Edit `settings.ini` to change default board
2. Or use CMake option: `cmake -DBOARD=<board_name>`
3. Board definitions are in `boards/bronco_space/`
4. Run `make clean` then `make generate build`

## Additional Repository Information

### Build Artifacts & Outputs

- **Firmware Binary**: `build-artifacts/zephyr.uf2` (for RP2040/RP2350 boards)
- **Firmware Hex**: `build-artifacts/zephyr.hex` (for STM32 boards)
- **Dictionary**: `build-artifacts/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json`
- **Build Cache**: `build-fprime-automatic-zephyr/` (F Prime + Zephyr build directory)

### Component Architecture

The project includes a comprehensive set of custom F Prime components organized by function:

**ADCS & Control**:
- **ADCS**: Attitude determination and control system
- **DetumbleManager**: Magnetic detumble control

**Communication**:
- **AmateurRadio**: Amateur radio (LoRa) management
- **SBand**: S-Band radio management
- **ComCcsdsLora / ComCcsdsSband / ComCcsdsUart**: CCSDS framing layers for each link
- **ComDelay**: Communication delay management
- **PayloadCom**: Payload communication interface

**Core System**:
- **ModeManager**: Spacecraft operating mode management
- **StartupManager**: Startup sequence and boot count
- **ResetManager**: System reset and watchdog management
- **Watchdog**: Hardware watchdog with LED indication
- **FatalHandler**: System error handling and recovery
- **BootloaderTrigger**: Bootloader mode entry for firmware updates

**Hardware Drivers (Drv/)**:
- **ImuManager**: LSM6DSO 6-axis IMU sensor management
- **Drv2605Manager**: Haptic feedback driver
- **Ina219Manager**: INA219 current/power monitor
- **RtcManager**: RV3028 real-time clock management
- **Tmp112Manager**: TMP112 temperature sensor
- **Veml6031Manager**: VEML6031 ambient light sensor

**Hardware Control**:
- **AntennaDeployer**: Antenna deployment mechanism
- **Burnwire**: Burnwire deployment control
- **CameraHandler**: Camera management
- **LoadSwitch**: Power load switch control
- **PowerMonitor**: Power system monitoring
- **ThermalManager**: Thermal monitoring and control

**Storage**:
- **FlashWorker**: Flash memory read/write management
- **FsFormat**: Filesystem formatting
- **FsSpace**: Filesystem space monitoring
- **NullPrmDb**: No-op parameter database (for systems without persistent storage)

**Security**:
- **Authenticate**: HMAC-based command authentication
- **AuthenticationRouter**: Routes authenticated vs. unauthenticated commands

### Development Environment

- **Python Version**: 3.13+ (specified in `.python-version`)
- **Package Manager**: UV v0.8.13 (automatically downloaded)
- **Virtual Environment**: `fprime-venv/` (created by Makefile)
- **Zephyr SDK**: ARM toolchain (installed to `~/zephyr-sdk-*`)
- **West Workspace**: `.west/` (Zephyr workspace configuration)

### Git Submodules

The repository uses three main submodules:

- `lib/fprime/` - F Prime framework (NASA's flight software framework)
- `lib/fprime-zephyr/` - F Prime-Zephyr integration layer
- `lib/fprime-extras/` - Additional F Prime libraries
- `lib/zephyr-workspace/` - Zephyr RTOS workspace

### Configuration Files

- `settings.ini` - F Prime project settings and default board
- `prj.conf` - Zephyr project configuration (USB, I2C, SPI, sensors)
- `CMakePresets.json` - CMake presets for different build configurations
- `fprime-gds.yml` - GDS command-line defaults
- `cpplint.cfg` - C++ linting configuration
- `.clang-format` - C/C++ formatting rules

### Development Tools (`tools/`)

The `tools/` directory contains development and analysis tools. See `tools/README.md` for full documentation.

**Data Budget Tool** (`tools/data_budget.py`):
Analyzes F Prime telemetry definitions to calculate serialized byte sizes of telemetry channels and packets. Essential for mission downlink budget planning.

```bash
make data-budget          # Summary: total channels, bytes per telemetry group
make data-budget VERBOSE=1 # Detailed: per-channel sizes, packet breakdowns
```

Output includes bytes per telemetry group (Beacon, Sensor Data, Meta Data, Payload, Health, Parameters).

### Framing Plugin (`Framing/`)

A pip-installable F Prime framing/deframing plugin implementing CCSDS packet framing. Install it with:

```bash
make framer-plugin
```

Required for `make gds-integration` (CI mode GDS). Installed into the virtual environment automatically.

### Command Sequences (`sequences/`)

Pre-defined F Prime command sequences for operational use. Compile a sequence with:

```bash
make sequence SEQ=startup        # Compile sequences/startup.seq
make sequence SEQ=enter_safe     # Compile sequences/enter_safe.seq
```

Available sequences: `startup`, `enter_safe`, `camera_handler_1`, `radio-fast`, `radio_enter_safe`, `your-face`, `not-your-face`.

After compiling, upload the sequence through GDS for execution on the board.

### Authentication & Security

Commands can be HMAC-authenticated using the `Authenticate` component. The authentication key is stored in `PROVESFlightControllerReference/Components/Authenticate/AuthDefaultKey.h`.

```bash
make generate-auth-key   # Generate a new random HMAC key (only if file doesn't exist)
make copy-secrets SECRETS_DIR=<dir>  # Copy production keys and auth key from a secure directory
```

Firmware is signed with MCUBoot for secure boot. The signing key is at `keys/proves.pem` (copied from the MCUBoot test key or a production key). Build signed firmware with:

```bash
make build-mcuboot       # Build firmware with MCUBoot signing
```

## Trust These Instructions

These instructions are comprehensive and validated. **Only search for additional information if**:

- Instructions are incomplete for your specific task
- You encounter errors not covered in "Common Issues"
- You need board-specific flashing instructions (see docs-site/uploading/ and docs-site/additional-resources/board-list.md)

For standard build/test/lint workflows, **trust and follow these instructions exactly** to minimize exploration time and command failures.
