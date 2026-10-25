---
title: Com Delay
category: Communication
---

# Components::ComDelay

`Components::ComDelay` is a parameterized rate group schedule divider. On the initial run invocation and every DIVIDER + 1 run invocations thereafter, any received com status is sent out. On the 10 Hz `downlinkDelay` rate group, the default DIVIDER of 299 releases every 30 s. This effectively delays the com status until the next (divided) run call.

# 1 Requirements

| Requirement ID | Description                                                          | Validation |
|----------------|----------------------------------------------------------------------|------------|
| COM_DELAY_001  | The `Svc::ComDelay` component shall accept com status in.            | Unit-Test  |
| COM_DELAY_002  | The `Svc::ComDelay` component shall emit com status once every DIVIDER + 1 rate group ticks, for every U16 DIVIDER value. | Unit-Test |
| COM_DELAY_003  | The `Svc::ComDelay` component shall set the DIVIDER via a parameter. | Unit-Test  |

# 2 Parameters

| Name    | Description |
|---------|-----------------------------------------------------------|
| DIVIDER | Status is sent every DIVIDER + 1 rate group ticks |
