# Components::ComDelay

`Components::ComDelay` is a parameterized rate group schedule divider. On the initial run invocation and on each multiple of the divider thereafter any received com status is sent out. This effectively delays the com status until the next (divided) run call.

# 1 Requirements

| Requirement ID | Description                                                          | Validation |
|----------------|----------------------------------------------------------------------|------------|
| COM_DELAY_001  | The `Svc::ComDelay` component shall accept com status in.            | Unit-Test  |
| COM_DELAY_002  | The `Svc::ComDelay` component shall emit com status once for each DIVIDER number of rate group ticks. | Unit-Test |
| COM_DELAY_003  | The `Svc::ComDelay` component shall set the DIVIDER via a parameter. | Unit-Test  |

# 2 Parameters

| Name    | Description |
|---------|-----------------------------------------------------------|
| DIVIDER | Number of rate group ticks received before sending status |
