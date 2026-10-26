/* SPDX-License-Identifier: Apache-2.0 */
#ifndef KLEO_INA260_CORE_H
#define KLEO_INA260_CORE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Transport returns zero on success, negative errno on failure. */
struct ina260_io {
    void *ctx;
    int (*read)(void *ctx, uint8_t reg, uint8_t bytes[2]);
    int (*write)(void *ctx, const uint8_t *bytes, size_t count);
    int64_t (*now_ms)(void *ctx);
    void (*sleep_ms)(void *ctx, uint32_t ms);
};
struct ina260_sample {
    bool valid;
    uint16_t current_raw, voltage_raw, power_raw;
    int64_t timestamp_ms;
};
int ina260_identify(const struct ina260_io *io);
int ina260_fetch(const struct ina260_io *io, struct ina260_sample *sample);
int64_t ina260_current_micro(uint16_t raw);
int64_t ina260_voltage_micro(uint16_t raw);
int64_t ina260_power_micro(uint16_t raw);
#endif
