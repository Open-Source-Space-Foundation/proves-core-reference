# PROVES Core Reference - Software Design Documents

Welcome to the PROVES Core Reference Software Design Documents (SDDs) site. This documentation provides detailed design information for all components in the PROVES Core Reference implementation.

## Overview

PROVES Core Reference is a reference software implementation combining F Prime (NASA's flight software framework) with Zephyr RTOS to create firmware for embedded flight control boards. The project targets ARM Cortex-M microcontrollers, specifically RP2350 (Raspberry Pi Pico 2) and STM32 boards.

## Component Categories

The documentation is organized into the following categories:

### [Core Components](components/ADCS.md)
System-level components that manage the overall operation and state of the spacecraft: [ADCS](components/ADCS.md), [Mode Manager](components/ModeManager.md), [Startup Manager](components/StartupManager.md), [Reset Manager](components/ResetManager.md), [Watchdog](components/Watchdog.md), [Bootloader Trigger](components/BootloaderTrigger.md), [Detumble Manager](components/DetumbleManager.md)

### [Communication Components](components/AmateurRadio.md)
Components that handle various communication protocols and interfaces: [Amateur Radio](components/AmateurRadio.md), [S-Band](components/SBand.md), [Com CCSDS UART](components/ComCcsdsUart.md), [Com CCSDS S-Band](components/ComCcsdsSband.md), [Payload Com](components/PayloadCom.md), [Com Delay](components/ComDelay.md)

### [Hardware Components](components/AntennaDeployer.md)
Components that interface with physical hardware devices and actuators: [Antenna Deployer](components/AntennaDeployer.md), [Burnwire](components/Burnwire.md), [Camera Handler](components/CameraHandler.md), [Load Switch](components/LoadSwitch.md)

### [Sensor Components](components/ImuManager.md)
Components that manage and process data from various sensors: [IMU Manager](components/ImuManager.md), [Power Monitor](components/PowerMonitor.md), [Thermal Manager](components/ThermalManager.md)

### [Driver Components](components/Drv2605Manager.md)
Low-level driver components for specific hardware peripherals: [Drv2605 Manager](components/Drv2605Manager.md), [Ina219 Manager](components/Ina219Manager.md), [RTC Manager](components/RtcManager.md), [Tmp112 Manager](components/Tmp112Manager.md), [Veml6031 Manager](components/Veml6031Manager.md)

### [Storage Components](components/FlashWorker.md)
Components that manage persistent storage and filesystem operations: [Flash Worker](components/FlashWorker.md), [Fs Format](components/FsFormat.md), [Fs Space](components/FsSpace.md), [Null Prm Db](components/NullPrmDb.md)

### [Security Components](components/Authenticate.md)
Components that handle authentication and security features: [Authenticate](components/Authenticate.md), [Authentication Router](components/AuthenticationRouter.md)

## Navigation

Use the navigation tabs above to browse through the different component SDDs. Each SDD includes:

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
