---
name: Build/Integration Issue
about: Report problems with the build system, CMake, Zephyr integration, or toolchain
title: "[BUILD]: "
labels: build, integration
---

Use this template for issues related to building, compiling, linking, or integrating the F Prime Zephyr project.

## Build Stage
<!-- At what stage does the build fail? -->
<!-- Options: -->
<!-- - Submodule initialization (make submodules) -->
<!-- - Python environment setup (make fprime-venv) -->
<!-- - Zephyr setup (make zephyr) -->
<!-- - F Prime generation (make generate) -->
<!-- - Build/Compilation (make build) -->
<!-- - Linking -->
<!-- - Flashing/Upload -->
<!-- - Runtime/Execution -->
<!-- - Testing (make test-integration) -->
<!-- - Other (specify in description) -->

## Build System/Tool Versions
<!-- Provide version information for your build environment -->
<!-- Example:
- OS: Ubuntu 22.04 / macOS 14.0 / Windows 11 + WSL2
- Python version: 3.13.0 (from `python --version`)
- CMake version: 3.28.0 (from `cmake --version`)
- GCC/ARM toolchain: (from `arm-none-eabi-gcc --version` or Zephyr SDK version)
- UV version: 0.8.13 (from `uv --version`)
- F Prime version: v4.0.0a1 (from lib/fprime/ git tag)
- Zephyr version: v4.2.0 (from west.yml)
- Make version: (from `make --version`)
-->

## Build Configuration
<!-- What is your build configuration? -->
<!-- Example:
- Board: proves_flight_control_board_v5d/rp2350a/m33
- Build type: Release
- Custom CMake options: (if any)
- Environment variables: (if any special settings)
- Settings from settings.ini: (if modified)
-->

## Error Messages
<!-- Paste the complete error output from the build -->
```
[build output here]
```

## Relevant CMakeLists.txt Snippet
<!-- If the issue relates to CMake configuration, paste the relevant section -->
```cmake
# From PROVESFlightControllerReference/Components/MyComponent/CMakeLists.txt
# Paste relevant CMake code here
```

## Relevant prj.conf Snippet
<!-- If the issue relates to Zephyr configuration, paste the relevant section -->
```
# From prj.conf
# Paste relevant Zephyr configuration here
```

## Steps to Reproduce
<!-- Provide exact steps to reproduce the build issue -->
<!-- Example:
1. Fresh clone: `git clone https://github.com/Open-Source-Space-Foundation/proves-core-reference`
2. Run: `make submodules`
3. Run: `make fprime-venv`
4. Run: `make zephyr`
5. Run: `make generate`
6. Run: `make build`
7. Error occurs at step 6...
-->

## Clean Build Test
<!-- Did you try a clean build? -->
<!-- Options: -->
<!-- - Yes, I ran 'make clean' and rebuilt -->
<!-- - No, I haven't tried a clean build yet -->
<!-- - Not applicable -->

## Workaround
<!-- Have you found any workarounds for this issue? -->

## Additional Build Logs
<!-- If available, paste additional logs (CMake configure output, verbose build output) -->
```
# Output from: make build VERBOSE=1
```

## Affects CI/CD
<!-- Does this issue affect continuous integration builds? -->
<!-- Options: -->
<!-- - Yes, CI builds are failing -->
<!-- - No, only affects local builds -->
<!-- - Unknown -->

## Additional Context
<!-- Any other relevant information about the build issue -->
