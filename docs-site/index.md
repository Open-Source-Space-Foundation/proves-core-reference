# PROVES Core Reference - Software Design Documents

Welcome to the PROVES Core Reference Software Design Documents (SDDs) site. This documentation provides detailed design information for all components in the PROVES Core Reference implementation.

## Overview

PROVES Core Reference is a reference software implementation combining F Prime (NASA's flight software framework) with Zephyr RTOS to create firmware for embedded flight control boards. The project targets ARM Cortex-M microcontrollers, specifically RP2350 (Raspberry Pi Pico 2) and STM32 boards.

## Component Categories

The documentation is organized into the following categories:

### Core Components
System-level components that manage the overall operation and state of the spacecraft.

### Communication Components
Components that handle various communication protocols and interfaces.

### Hardware Components
Components that interface with physical hardware devices and actuators.

### Sensor Components
Components that manage and process data from various sensors.

### Driver Components
Low-level driver components for specific hardware peripherals.

### Storage Components
Components that manage persistent storage and filesystem operations.

### Security Components
Components that handle authentication and security features.

## Navigation

Use the navigation menu on the left to browse through the different component SDDs. Each SDD includes:

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
