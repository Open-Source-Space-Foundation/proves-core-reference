# Components::ReadinessMonitor

ReadinessMonitor answers "is this spacecraft ready to fly?" from inside the spacecraft. It rolls up values other components already publish into three masks in the Beacon packet. It only reports. It sends no commands, gates nothing (deploy, radio, and safe-mode exit never wait on it), and never emits FATAL. `Svc.Health` remains the only liveness and reset path.

The ground readiness suite runs `RUN_CHECKS`, waits for `ReadinessSummary`, and asserts `FailMask == 0`.

## How it reads other components

| Source | Port | Used for |
|---|---|---|
| `CdhCore.tlmSend` (TlmPacketizer) | `tlmGet` → `TlmGet` | Latest value of channels in `Sources` |
| `FileHandling.prmDb` | `prmDbGet` → `getPrm` | Stored `StartupManager` parameters |
| TMP112 and VEML6031 managers | `tempGet`, `lightGet` | The same getters `thermalManager` and `adcs` call, only on a full run |
| The file system | `Os::Directory`, `Os::FileSystem` | Root listing and file presence, only on a full run |

TlmPacketizer returns nothing for channels in the packet omit list, so every channel in `Sources` must be in a packet. `watchdog.WatchdogTransitions` moved to `HealthAuxiliary` for this reason. A channel that has never been written reads as "no data".

## Cadence

- `run` (1 Hz rate group): the cheap checks. Stall limits count calls, so the port must be on the 1 Hz group.
- `RUN_CHECKS`, and once `BOOT_FULL_RUN_DELAY_S` after boot: the cheap checks plus the root listing, flight config, and one read of every powered sensor. Then `ReadinessSummary`.

All async inputs drop on a full queue, so a stalled monitor can never assert.

## Masks

Bit *n* of `PassMask`, `WarnMask`, and `FailMask` is `ReadinessCheck` *n*. A check sits in at most one mask; in none means `NOT_RUN`. Bit positions are append only.

| Bit | Check | Cadence | WARN | FAIL | `detail` |
|---|---|---|---|---|---|
| 0 | `FS_ROOT` | full | | root unreadable, a name with control or non-ASCII bytes, or no `boot_count.bin` | bits: 1 unreadable, 2 garbage name, 4 no boot count |
| 1 | `FS_SPACE` | 1 Hz | free < `FS_FREE_FLOOR_BYTES` | no `FreeSpace` value | free KiB |
| 2 | `DOWNLINK` | 1 Hz | a file buffer has sat in the LoRa file queue for `DOWNLINK_STALL_S` | | seconds held |
| 3 | `FACE_SENSORS` | full | | a powered sensor failed its read | `SensorFailMask` |
| 4 | `MTQ_DRIVERS` | | always `NOT_RUN`: `Drv2605Manager` has no status port yet | | |
| 5 | `I2C_BUS` | full | every sensor on one mux channel failed | every powered mux channel failed | bitmask of dead mux channels |
| 6 | `WDT_PET` | 1 Hz | | `WatchdogTransitions` unchanged for `WDT_STALL_S` | seconds unchanged |
| 7 | `RESET_CAUSE` | | always `NOT_RUN`: nothing reads the MCU reset cause yet | | |
| 8 | `BATTERY` | 1 Hz | voltage < `VBATT_WARN_V` | no `Voltage` value | millivolts |
| 9 | `FLIGHT_CONFIG` | full | | `ARMED` false, `QUIESCENCE_TIME` < 2700 s, `TRANSMIT_ENABLE_TICKS` < 2700, or `/seq/startup.bin` missing or empty | bits: 1 disarmed, 2 short quiescence, 4 short TX enable, 8 no startup sequence |
| 10 | `CLOCK_BOOT` | 1 Hz | `DisciplineReadFaults` rose | `BootCount` missing or 0 | |
| 11 | `RADIO` | 1 Hz | after quiescence, `lora.BytesSent` unchanged for `RADIO_SILENT_S` | | seconds silent |

`NOT_RUN` also covers `FACE_SENSORS` and `I2C_BUS` when no sensor is powered, and `RADIO` while quiescence runs or when its end time is not comparable with the current time.

`SensorFailMask` bit order follows the port order: TMP112 faces 0, 1, 2, 3, 5 are bits 0 to 4, battery cells 1 to 4 are bits 5 to 8, VEML6031 faces 0, 1, 2, 3, 5 are bits 9 to 13.

## Events

- `CheckDegraded` (WARNING_HI) when a check moves to WARN or FAIL.
- `CheckCleared` (ACTIVITY_HI) when a check moves to PASS or NOT_RUN.
- `ReadinessSummary` (ACTIVITY_HI) after each full run.

Events fire on a change of result only, and are never throttled.

## Limits

- `BATTERY` reads the last value `ina219SysManager` published. An INA219 that dies after its first good read keeps reporting that value.
- A powered face that was switched on less than a second ago reads as failed, because its driver waits one second after power-on.
- `FLIGHT_CONFIG` treats a parameter missing from PrmDb as running at its FPP default, which is the flight value.
