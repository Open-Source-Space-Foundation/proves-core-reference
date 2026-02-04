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
# This runs: pytest FprimeZephyrReference/test/int --deployment build-artifacts/zephyr/fprime-zephyr-deployment
```

**Test Framework Details**:

- **Location**: `FprimeZephyrReference/test/int/`
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
fprime-gds.yaml        # GDS command-line defaults
requirements.txt       # Python dependencies
.python-version        # Python version requirement (3.13)
```

### Directory Structure

```
FprimeZephyrReference/
├── Components/        # Custom F Prime components
│   ├── BootloaderTrigger/
│   ├── Drv/          # Driver components (IMU, RTC, sensor managers)
│   ├── FatalHandler/
│   ├── DetumbleManager/
│   └── Watchdog/
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
└── zephyr-workspace/ # Zephyr RTOS (404MB, git submodule)

boards/               # Custom board definitions
└── bronco_space/
    ├── proves_flight_control_board_v5/     # Base board definition
    ├── proves_flight_control_board_v5c/    # Variant C (LED on GPIO 24)
    └── proves_flight_control_board_v5d/    # Variant D (standard configuration)

docs/
├── main-content/     # Setup and build documentation
└── additional-resources/  # Board-specific guides, troubleshooting
```

### Key Architecture Points

**F Prime + Zephyr Integration**:

- F Prime components defined in `.fpp` files (autocoded to C++)
- Zephyr handles RTOS, drivers, and hardware abstraction
- Main entry point: `FprimeZephyrReference/ReferenceDeployment/Main.cpp`
  - **Critical**: 3-second sleep before starting to allow USB CDC ACM initialization
- Build system: CMake with F Prime and Zephyr toolchains
- Default board: `proves_flight_control_board_v5c/rp2350a/m33` (configurable in `settings.ini`)

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
make zephyr      # Set up Zephyr workspace and ARM toolchain
make generate          # Generate F Prime build cache (force)
make generate-if-needed # Generate only if build directory missing
make build             # Build firmware (runs generate-if-needed)
make fmt               # Run linters and formatters (pre-commit)
make gds               # Start F Prime Ground Data System
make gds-integration   # Start GDS without GUI (for CI)
make test-integration  # Run integration tests
make bootloader        # Trigger bootloader mode on RP2350
make clean             # Remove all gitignored files
make uv                # Download UV package manager
make pre-commit-install # Install pre-commit hooks
make download-bin      # Download binary tools (internal)
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

This automatically detects if the board is already in bootloader mode and triggers it if needed. See board-specific guides in `docs/additional-resources/board-list.md`.

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
- See `docs/additional-resources/board-list.md` for tested boards

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

- `FprimeZephyrReference/ReferenceDeployment/Top/ReferenceDeploymentTopology.cpp` - Main topology implementation
- `FprimeZephyrReference/ReferenceDeployment/Top/topology.fpp` - FPP topology definition
- `FprimeZephyrReference/ReferenceDeployment/Top/instances.fpp` - Component instances

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

1. Edit `FprimeZephyrReference/ReferenceDeployment/Top/instances.fpp` for new component instances
2. Edit `FprimeZephyrReference/ReferenceDeployment/Top/topology.fpp` for connections
3. Run `make generate build`

### When adding new components:

1. Create component directory under `FprimeZephyrReference/Components/`
2. Add `CMakeLists.txt` with `register_fprime_library()` or `register_fprime_module()`
3. Add component to parent `CMakeLists.txt` with `add_fprime_subdirectory()`
4. Follow existing component structure (see `Components/Watchdog/` as example)

### ReferenceDeployment Topology and DT_NODE Integration

**Understanding the Topology System**:
The `ReferenceDeploymentTopology` serves as the central coordinator that bridges F Prime components with Zephyr hardware drivers through Device Tree nodes.

**Key Topology Files**:

- `FprimeZephyrReference/ReferenceDeployment/Top/ReferenceDeploymentTopology.cpp` - Main implementation
- `FprimeZephyrReference/ReferenceDeployment/Top/topology.fpp` - FPP topology definition
- `FprimeZephyrReference/ReferenceDeployment/Top/instances.fpp` - Component instances
- `FprimeZephyrReference/ReferenceDeployment/Top/ReferenceDeploymentTopologyDefs.hpp` - Type definitions

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

The project includes several custom F Prime components:

- **Watchdog**: Hardware watchdog management with LED indication
- **IMU Manager**: LSM6DSO 6-axis IMU sensor management
- **LIS2MDL Manager**: 3-axis magnetometer management
- **RTC Manager**: RV3028 real-time clock management
- **Bootloader Trigger**: Bootloader mode entry functionality
- **Fatal Handler**: System error handling and recovery

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
- `lib/zephyr-workspace/` - Zephyr RTOS workspace

### Configuration Files

- `settings.ini` - F Prime project settings and default board
- `prj.conf` - Zephyr project configuration (USB, I2C, SPI, sensors)
- `CMakePresets.json` - CMake presets for different build configurations
- `fprime-gds.yaml` - GDS command-line defaults
- `cpplint.cfg` - C++ linting configuration
- `.clang-format` - C/C++ formatting rules

## Trust These Instructions

These instructions are comprehensive and validated. **Only search for additional information if**:

- Instructions are incomplete for your specific task
- You encounter errors not covered in "Common Issues"
- You need board-specific flashing instructions (see docs/)

For standard build/test/lint workflows, **trust and follow these instructions exactly** to minimize exploration time and command failures.
