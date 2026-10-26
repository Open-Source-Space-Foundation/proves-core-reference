# KLEO INA260 module — bench-development candidate

Independent local Zephyr module and two-device sample for PROVES V5c. Does not
modify upstream INA219. Source authority: TI INA260 datasheet SBOS656C, sections
8.6.1–8.6.8; https://www.ti.com/lit/ds/symlink/ina260.pdf.

## Behavior

- Standard Zephyr sensor fetch/get API: volts, signed amps, unsigned watts.
- I2C1 GPIO2/GPIO3 at 100 kHz; sample addresses 0x40 and 0x41.
- Initialization checks TI manufacturer 0x5449 and masked die ID 0x2270 only.
- Each fetch identifies the device, writes configuration 0x6123, verifies it,
  and waits for conversion ready in register 0x06. One average, both conversions
  at 1.1 ms, triggered mode. This intentionally changes sensor configuration when
  the sample is run. It does not reset the sensor or write calibration registers.
- Poll deadline is 20 ms; individual I2C calls rely on the controller driver's
  timeout. This is not a guarantee against a blocked controller transport.
- Reads current/voltage/power individually from 0x01/0x02/0x03 after completion.
  Scales: 1250 microamps/count, 1250 microvolts/count, 10000 microwatts/count.
- A failed fetch invalidates the cache and leaves old raw values uncommitted.
  channel_get returns -ENODATA until a successful fetch. Unsupported channels
  return -ENOTSUP. Overflow/invalid voltage returns -ERANGE.
- Reading status consumes CVRF. Driver must exclusively own sensor status and
  configuration; do not concurrently use a second driver or raw register client.
- Per-instance mutex protects each API call. Application must serialize the
  full fetch-plus-get sequence (the supplied sample uses one thread).
- An initialized device can recover from a later power cycle on its next fetch.
  If initial device initialization fails, automatic re-initialization is not
  implemented. No runtime attributes, ALERT interrupt, or PM hooks yet.

## Testing and deployment

Run `bash test-in-wsl.sh` for the core tests plus five Zephyr native simulation
tests of the real sensor API through emulated I2C. See [SIMULATION.md](SIMULATION.md)
for coverage and the proposed component and system testing layers.

Run build-in-wsl.sh to test the actual portable C core with an injected I2C
transport, then compile the module/sample against the existing Zephyr 4.4.2
reference checkout. A fresh workspace is created for each run. Tests exercise
conversion boundaries, wrong device, every read-failure position, write failure,
config mismatch, timeout, overflow, partial-sample failure, and two instances.
These do not validate the Zephyr wrapper concurrency or physical measurements.

The sample performs ten acquisition rounds at both addresses and prints raw
micro-units without floating-point formatting. Every error is reported as invalid.

BUILD ONLY. Do not flash the generated UF2 yet. The inherited board startup,
external watchdog, console, flash layout, and MCUboot signing/recovery procedure
still need deployment review. The full-flash snapshot has been verified against
the device but restoration has not been tested. No F Prime or GDS integration is
included yet. Do not label measurements as battery/total spacecraft power until
the EPS measurement paths are confirmed.

Other components should use separate focused tests. Do not enable deployment,
heater, load-switch, or radio operations as part of sensor bring-up.
