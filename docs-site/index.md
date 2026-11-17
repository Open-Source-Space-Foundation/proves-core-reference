# PROVES Core Reference - Software Design Documents

Welcome to the PROVES Core Reference documentation site. This documentation provides comprehensive information for working with PROVES Core Reference, from getting started to detailed component design documents.

## Overview

PROVES Core Reference is a reference software implementation combining F Prime (NASA's flight software framework) with Zephyr RTOS to create firmware for embedded flight control boards. The project targets ARM Cortex-M microcontrollers, specifically RP2350 (Raspberry Pi Pico 2) and STM32 boards.

## Documentation Sections

### [Getting Started](getting-started/initial-setup.md)
Step-by-step guides to set up your development environment, build the firmware, and flash it to your board:

- [Initial Setup](getting-started/initial-setup.md)
- Clone the repository and set up dependencies
- [Build, Flash, and Run](getting-started/build-flash-run.md)
- Build and deploy the firmware

### Component Software Design Documents (SDDs)

Every component publishes its design document from its own `docs/` folder. Browse them by category in the **Component SDDs** menu, starting with the [Mode Manager](components/ModeManager/sdd.md).

### [Additional Resources](additional-resources/board-list.md)
Troubleshooting guides and additional configuration information:

- [Board List](additional-resources/board-list.md)
- Tested boards and their configurations
- [Specifying Board Configuration](additional-resources/specifying-board-configuration.md)
- [Troubleshooting](additional-resources/troubleshooting.md)
- [WSL Notes](additional-resources/wsl-notes.md)
- [Self-Hosted Runner Setup](additional-resources/self-hosted-runner-setup.md)

## Navigation

Use the **Component SDDs** tab above to browse the component SDDs. Each SDD includes:

- Introduction and purpose
- Requirements
- Design details
- Port definitions
- Command and event specifications
- Telemetry information
- Usage examples

## Additional Resources

- [Main Repository](https://github.com/Open-Source-Space-Foundation/proves-core-reference)
- [F Prime Documentation](https://fprime.jpl.nasa.gov/)
- [Zephyr RTOS Documentation](https://docs.zephyrproject.org/)
