# Components::ZephyrPrmDb

Zephyr parameter database.  Returns error status on any call.

## Requirements

| Name | Description | Validation |
|---|---|---|
|ZEPHYR-PRM-DB-001| Return error on any port call | Inspection|

## Usage Examples

Add the instance to a topology:

```
param connections instance ZephyrPrmDb
```


## Port Descriptions
| Name | Description |
|---|---|
| getPrm | Get parameter, returns `Fw::ParamValid::INVALID` |
| setPrm | No-op |
