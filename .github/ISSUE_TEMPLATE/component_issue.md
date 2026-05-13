---
name: Component Issue
about: Report issues specific to F Prime components (ports, threading, FPP, etc.)
title: "[COMPONENT]: "
labels: component
---

Use this template for issues related to F Prime component design, implementation, or behavior.

## Component Name

<!-- Name of the component (e.g., MagnetorquerManager, Watchdog, AntennaDeployer) -->

## Component Type

<!-- What type of F Prime component is this? -->
<!-- Options: -->
<!-- - Active Component (has thread) -->
<!-- - Passive Component (no thread) -->
<!-- - Queued Component -->
<!-- - Driver Component -->
<!-- - Other (specify in description) -->

## Component Location

<!-- Path to the component in the repository -->
<!-- Example: PROVESFlightControllerReference/Components/MagnetorquerManager -->

## Issue Type

<!-- What aspect of the component is problematic? (can select multiple) -->
<!-- Options: -->
<!-- - Port Connections (input/output ports) -->
<!-- - Port Invocation/Behavior -->
<!-- - Thread/Task Configuration -->
<!-- - Priority/Stack Size Issues -->
<!-- - Component State Management -->
<!-- - FPP Definition Errors -->
<!-- - Command Handler Issues -->
<!-- - Telemetry Channel Issues -->
<!-- - Event Reporting Issues -->
<!-- - Parameter Handling -->
<!-- - Autocoded Files (regeneration needed) -->
<!-- - Component Lifecycle (init, startup, shutdown) -->
<!-- - Other (specify in description) -->

## Port Interaction Issues

<!-- If this relates to port interactions, describe the ports involved and the issue -->
<!-- Example:
- Port name: sensorDataOut
- Port type: output, async
- Connected to: DataLogger.sensorDataIn
- Issue: Port invocation causes deadlock when called from ISR context...
-->

## Thread/Priority Configuration

<!-- If this relates to threading, describe the configuration and issue -->
<!-- Example:
- Thread priority: 50
- Stack size: 4096 bytes
- Issue: Component task preempts critical system tasks, causing missed deadlines...
-->

## FPP Definition Issues

<!-- If this relates to FPP files, paste relevant FPP code and describe the issue -->

```fpp
# From ComponentName.fpp
# Paste relevant FPP code here
```

## Error Messages

<!-- Paste any relevant error messages, compiler warnings, or runtime errors -->

```
Paste error messages here...
```

## Steps to Reproduce

<!-- How can we reproduce this component issue? -->

## Expected Component Behavior

<!-- How should the component behave? -->

## Actual Component Behavior

<!-- What is the component actually doing? -->

## Topology Excerpt

<!-- Paste relevant parts of topology.fpp or instances.fpp if applicable -->

```fpp
# Paste topology connections here
```

## Proposed Fix

<!-- If you have ideas on how to fix this, please describe them -->

## Additional Context

<!-- Any other relevant information about this component issue -->
