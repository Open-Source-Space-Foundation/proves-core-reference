# Validation record

Software checks performed October 14, 2026. No board access was performed.

WSL build workspace:
`/home/asr/projects/proves-v5c-reference/ina260-driver.svHuGT`

## Passed

- Production C core compiled with GCC C11, -Wall -Wextra -Werror -pedantic,
  undefined-behavior sanitizer and no sanitizer recovery.
- All five test groups passed: numeric boundaries; IDs/endian/revision mask;
  transport/readback/atomic cache; timeout/overflow/invalid voltage; recovery
  and two independent instances.
- V5c `proves_flight_control_board_v5c/rp2350a/m33` sample compiled and linked
  with Zephyr 4.4.2 (`dccb0959963`), Zephyr SDK 1.0.1 and GNU ARM 14.3.0.
- Generated configuration enables CONFIG_KLEO_INA260, CONFIG_I2C and CONFIG_SENSOR.
  CONFIG_INA219 is not enabled in the configuration check.
- Linker reports 49,316 bytes flash, 15,640 bytes RAM.

## Remaining limitations

- No physical INA260 identity or measurements validated. No meter comparison.
- No Zephyr API runtime/concurrency tests; core tests use a simulated transport.
- Board defaults still produce two harmless-to-compilation configuration warnings
  for TCA954x initialization priorities after that unrelated driver is disabled.
- MCUboot warning: no signing key or unsigned-image generation selected. Result
  is a build artifact, not a deployment-ready image.
- Inherited pin ownership, output states, external watchdog and USB console must
  be reviewed before hardware execution. Restoration of the saved FC flash image
  has not been tested. No F Prime manager/GDS dictionary integration yet.

The earlier identification-only probe remains separate and should precede a
measurement application on hardware.
