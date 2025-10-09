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
make zephyr-setup

# Step 4: Generate F Prime build cache (~1 minute)
make generate

# Step 5: Build the firmware (~2-3 minutes)
make build
```

**IMPORTANT**: The `make` command (default target) runs all of the above automatically.

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

### Testing

**Integration Tests**:
Integration tests require the GDS (Ground Data System) to be running:

```bash
# Terminal 1: Start GDS
make gds

# Terminal 2: Run integration tests
make test-integration
```

**Test Location**: `FprimeZephyrReference/test/int/`
**Test Framework**: pytest with fprime-gds testing API

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
│   ├── ImuManager/
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
    └── proves_flight_control_board_v5*/

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
make zephyr-setup      # Set up Zephyr workspace and ARM toolchain
make generate          # Generate F Prime build cache (force)
make build             # Build firmware (runs generate-if-needed)
make fmt               # Run linters and formatters
make gds               # Start F Prime Ground Data System
make test-integration  # Run integration tests
make clean             # Remove all gitignored files
make clean-zephyr      # Remove Zephyr build files only
make clean-zephyr-sdk  # Remove Zephyr SDK (requires re-running zephyr-setup)
```

### CI/CD Pipeline (`.github/workflows/ci.yaml`)

**Jobs**:
1. **Lint**: Runs `make fmt` (pre-commit checks)
2. **Build**: Full build with caching
   - Caches: bin tools, submodules, Python venv, Zephyr workspace
   - Runs: `make submodules`, `make fprime-venv`, `make zephyr-setup`, `make generate-ci build-ci`
   - Uploads: `build-artifacts/zephyr.uf2` and dictionary JSON

**Critical for CI Success**:
- Always run `make fmt` before pushing
- Ensure code builds with `make build` locally
- Integration tests are NOT run in CI (require hardware)

## Common Issues & Workarounds

### Issue: Build Fails with "west not found"
**Solution**: Run `make zephyr-setup` to install west and Zephyr SDK.

### Issue: "No such file or directory: fprime-util"
**Solution**: Run `make fprime-venv` to create virtual environment with dependencies.

### Issue: CMake cache errors after changing board configuration
**Solution**: Run `make clean` followed by `make generate build`.

### Issue: USB device not detected on board
**Workaround**: The board may need to be put into bootloader mode. See board-specific guides in `docs/additional-resources/board-list.md`.

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

### When modifying topology:
1. Edit `FprimeZephyrReference/ReferenceDeployment/Top/instances.fpp` for new component instances
2. Edit `FprimeZephyrReference/ReferenceDeployment/Top/topology.fpp` for connections
3. Run `make generate build`

### When adding new components:
1. Create component directory under `FprimeZephyrReference/Components/`
2. Add `CMakeLists.txt` with `register_fprime_library()` or `register_fprime_module()`
3. Add component to parent `CMakeLists.txt` with `add_fprime_subdirectory()`
4. Follow existing component structure (see `Components/Watchdog/` as example)

### When modifying board configuration:
1. Edit `settings.ini` to change default board
2. Or use CMake option: `cmake -DBOARD=<board_name>`
3. Board definitions are in `boards/bronco_space/`
4. Run `make clean` then `make generate build`

## Trust These Instructions

These instructions are comprehensive and validated. **Only search for additional information if**:
- Instructions are incomplete for your specific task
- You encounter errors not covered in "Common Issues"
- You need board-specific flashing instructions (see docs/)

For standard build/test/lint workflows, **trust and follow these instructions exactly** to minimize exploration time and command failures.
