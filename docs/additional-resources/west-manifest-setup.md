# West Manifest Setup for RP2040/RP2350

This project uses a minimal West manifest (`west.yml`) optimized specifically for Raspberry Pi RP2040 and RP2350 builds. This significantly reduces setup time and disk space compared to the full Zephyr workspace.

## What's Included

The minimal manifest includes only the essential modules for RP2040/RP2350:

- **Zephyr RTOS core** (v4.2.0)
- **hal_rpi_pico** - Raspberry Pi Pico Hardware Abstraction Layer (REQUIRED)
- **hal_st** - STMicroelectronics HAL (for LIS2MDL, LSM6DSO sensors)
- **cmsis** - ARM CMSIS support for Cortex-M processors
- **cmsis_6** - ARM CMSIS v6 (required for Cortex-M33 in RP2350)
- **picolibc** - Lightweight C library
- **loramac-node** - LoRa/LoRaWAN support (for SX127x radios)
- **mbedtls** - Cryptographic library
- **tinycrypt** - Lightweight cryptographic library
- **mcuboot** - Bootloader support

## Fresh Installation

### Option A: Automated Setup (Recommended)

Use the convenience script that handles everything:

```bash
./scripts/setup-minimal-zephyr.sh
```

This script will:
1. Initialize the West workspace
2. Download only the minimal modules
3. Export the Zephyr CMake package
4. Install only the ARM toolchain (~95 MB instead of ~1.2 GB)

### Option B: Manual Setup

If you prefer to run the commands manually:

```bash
# Initialize west workspace with this manifest
west init -l .

# Update all projects (downloads only the minimal set of modules)
west update

# Export Zephyr CMake package
west zephyr-export

# Install Zephyr SDK with ONLY the ARM toolchain (for RP2040/RP2350)
# This avoids downloading 500+ MB of unnecessary toolchains
cd lib/zephyr-workspace/zephyr
python3 scripts/sdk-install/sdk_install.py --toolchains arm-zephyr-eabi
cd ../../..
```

## Migrating from Full Zephyr Workspace

If you already have a full Zephyr workspace installed:

### Option 1: Clean Start (Recommended)

```bash
# Backup any local changes first!

# Remove the old workspace
rm -rf lib/zephyr-workspace

# Initialize with the new minimal manifest
west init -l .

# Update with minimal modules
west update

# Export Zephyr CMake package
west zephyr-export

# Install only the ARM toolchain
cd lib/zephyr-workspace/zephyr
python3 scripts/sdk-install/sdk_install.py --toolchains arm-zephyr-eabi
cd ../../..
```

### Option 2: Update Existing Workspace

```bash
# Update west configuration to use the new manifest
west config manifest.path .

# Update all projects
west update
```

## Disk Space Savings

- **Full Zephyr workspace**: ~3-5 GB
- **Minimal RP2040/RP2350 workspace**: ~500-800 MB

This represents approximately **80-85% reduction** in disk space and download time.

## Zephyr SDK Optimization

The default Zephyr SDK installation downloads toolchains for all supported architectures (~1+ GB). Since this project only targets RP2040/RP2350 (ARM Cortex-M), you only need the `arm-zephyr-eabi` toolchain (~95 MB).

### Installing Only the ARM Toolchain

Instead of running the default SDK setup, use the selective installation:

```bash
cd lib/zephyr-workspace/zephyr
python3 scripts/sdk-install/sdk_install.py --toolchains arm-zephyr-eabi
```

This installs **only**:
- `arm-zephyr-eabi` - ARM Cortex-M toolchain (required for RP2040/RP2350)

This avoids downloading unnecessary toolchains for:
- aarch64, arc64, arc, microblazeel, mips, nios2, riscv64, rx, sparc, x86_64, xtensa

### SDK Space Savings

- **Full SDK (all toolchains)**: ~1.2 GB
- **ARM-only SDK**: ~95 MB

This represents a **~92% reduction** in SDK size.

### Reinstalling SDK with Minimal Toolchains

If you've already installed the full SDK and want to reclaim disk space:

```bash
# Remove existing SDK (backs up ~1+ GB of space)
rm -rf ~/zephyr-sdk-*

# Install minimal SDK with only ARM toolchain
cd lib/zephyr-workspace/zephyr
python3 scripts/sdk-install/sdk_install.py --toolchains arm-zephyr-eabi
cd -
```

**Note**: Your existing builds will continue to work after reinstalling with the ARM-only toolchain since this project only uses RP2040/RP2350 boards.

## Adding Additional Modules

If your project requires additional Zephyr modules, you can add them to the `west.yml` file. For example:

### Adding Filesystem Support

```yaml
- name: littlefs
  path: lib/zephyr-workspace/modules/fs/littlefs
  groups:
    - fs
  revision: 8f5ca347843363882619d8f96c00d8dbd88a8e79
```

### Adding Display/GUI Support

```yaml
- name: lvgl
  revision: b03edc8e6282a963cd312cd0b409eb5ce263ea75
  path: lib/zephyr-workspace/modules/lib/gui/lvgl
```

After modifying `west.yml`, run:

```bash
west update
```

## Verifying Your Setup

To verify that your workspace is properly configured:

```bash
# Check west status
west list

# Try building for one of your boards
west build -b proves_flight_control_board_v5c FprimeZephyrReference/ReferenceDeployment
```

## Troubleshooting

### "Module not found" errors during build

If you encounter errors about missing modules during build, you may need to add that specific module to `west.yml`. Check the error message for the module name and add it following the pattern in the manifest.

### Reverting to Full Manifest

If you need the full Zephyr workspace:

```bash
# Update west configuration to use Zephyr's manifest
west config manifest.path lib/zephyr-workspace/zephyr

# Update to get all modules
west update
```

## Module Version Updates

To update module versions:

1. Check the latest revisions in [Zephyr's west.yml](https://github.com/zephyrproject-rtos/zephyr/blob/main/west.yml)
2. Update the `revision` field for each module in your `west.yml`
3. Run `west update`

## References

- [West Manifest Documentation](https://docs.zephyrproject.org/latest/develop/west/manifest.html)
- [Zephyr Getting Started](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)
