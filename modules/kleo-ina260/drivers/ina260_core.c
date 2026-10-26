/* SPDX-License-Identifier: Apache-2.0
 * Register/scaling authority: TI INA260 SBOS656C, sections 8.6.1-8.6.8.
 */
#include "ina260_core.h"
#include <errno.h>

#define CONFIG_TRIGGERED 0x6123u
#define READY_BIT 0x0008u
#define OVERFLOW_BIT 0x0004u
#define CONVERSION_TIMEOUT_MS 20

static int read16(const struct ina260_io *io, uint8_t reg, uint16_t *value)
{
    uint8_t bytes[2];
    int rc = io->read(io->ctx, reg, bytes);
    if (rc != 0) { return rc; }
    *value = ((uint16_t)bytes[0] << 8) | bytes[1];
    return 0;
}

int ina260_identify(const struct ina260_io *io)
{
    uint16_t manufacturer, die;
    int rc = read16(io, 0xfe, &manufacturer);
    if (rc != 0) { return rc; }
    rc = read16(io, 0xff, &die);
    if (rc != 0) { return rc; }
    return manufacturer == 0x5449 && (die & 0xfff0) == 0x2270 ? 0 : -ENODEV;
}

int ina260_fetch(const struct ina260_io *io, struct ina260_sample *sample)
{
    /* Never publish a partial or stale sample as the latest valid reading. */
    sample->valid = false;
    int rc = ina260_identify(io);
    if (rc != 0) { return rc; }

    /* One average, 1.1 ms per measurement, triggered current + voltage.
     * Writing configuration starts a new conversion and clears CVRF.
     * No reset bit, INA219 calibration write, or alert configuration.
     */
    const uint8_t command[] = {0x00, CONFIG_TRIGGERED >> 8,
                               CONFIG_TRIGGERED & 0xff};
    rc = io->write(io->ctx, command, sizeof(command));
    if (rc != 0) { return rc; }
    const int64_t deadline = io->now_ms(io->ctx) + CONVERSION_TIMEOUT_MS;
    uint16_t config, status;
    rc = read16(io, 0x00, &config);
    if (rc != 0) { return rc; }
    if (config != CONFIG_TRIGGERED) { return -EIO; }

    for (;;) {
        if (io->now_ms(io->ctx) >= deadline) { return -ETIMEDOUT; }
        rc = read16(io, 0x06, &status);
        if (rc != 0) { return rc; }
        if (io->now_ms(io->ctx) >= deadline) { return -ETIMEDOUT; }
        if (status & OVERFLOW_BIT) { return -ERANGE; }
        if (status & READY_BIT) { break; }
        io->sleep_ms(io->ctx, 1);
    }

    struct ina260_sample next = {0};
    rc = read16(io, 0x01, &next.current_raw);
    if (rc != 0) { return rc; }
    rc = read16(io, 0x02, &next.voltage_raw);
    if (rc != 0) { return rc; }
    if (next.voltage_raw & 0x8000) { return -ERANGE; }
    rc = read16(io, 0x03, &next.power_raw);
    if (rc != 0) { return rc; }
    next.timestamp_ms = io->now_ms(io->ctx);
    next.valid = true;
    *sample = next;
    return 0;
}

int64_t ina260_current_micro(uint16_t raw)
{
    /* Explicit two's-complement conversion avoids implementation-defined casts. */
    int32_t signed_raw = raw & 0x8000 ? (int32_t)raw - 65536 : raw;
    return (int64_t)signed_raw * 1250;
}
int64_t ina260_voltage_micro(uint16_t raw) { return (int64_t)raw * 1250; }
int64_t ina260_power_micro(uint16_t raw) { return (int64_t)raw * 10000; }
