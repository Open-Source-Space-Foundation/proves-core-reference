/* SPDX-License-Identifier: Apache-2.0 */
#define DT_DRV_COMPAT ti_ina260
#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include "ina260_core.h"

struct ina260_config { struct i2c_dt_spec bus; };
struct ina260_data {
    struct k_mutex lock;
    struct ina260_sample sample;
};
static int bus_read(void *ctx, uint8_t reg, uint8_t bytes[2])
{
    const struct device *dev = ctx;
    const struct ina260_config *cfg = dev->config;
    return i2c_write_read_dt(&cfg->bus, &reg, 1, bytes, 2);
}
static int bus_write(void *ctx, const uint8_t *bytes, size_t count)
{
    const struct device *dev = ctx;
    const struct ina260_config *cfg = dev->config;
    return i2c_write_dt(&cfg->bus, bytes, count);
}
static int64_t now_ms(void *ctx) { ARG_UNUSED(ctx); return k_uptime_get(); }
static void sleep_ms(void *ctx, uint32_t ms) { ARG_UNUSED(ctx); k_msleep(ms); }
static struct ina260_io transport(const struct device *dev)
{
    return (struct ina260_io){.ctx = (void *)dev, .read = bus_read,
        .write = bus_write, .now_ms = now_ms, .sleep_ms = sleep_ms};
}
static int sample_fetch(const struct device *dev, enum sensor_channel channel)
{
    if (channel != SENSOR_CHAN_ALL && channel != SENSOR_CHAN_VOLTAGE &&
        channel != SENSOR_CHAN_CURRENT && channel != SENSOR_CHAN_POWER) {
        return -ENOTSUP;
    }
    struct ina260_data *data = dev->data;
    struct ina260_io io = transport(dev);
    k_mutex_lock(&data->lock, K_FOREVER);
    int rc = ina260_fetch(&io, &data->sample);
    k_mutex_unlock(&data->lock);
    return rc;
}
static int channel_get(const struct device *dev, enum sensor_channel channel,
                       struct sensor_value *value)
{
    if (channel != SENSOR_CHAN_VOLTAGE && channel != SENSOR_CHAN_CURRENT &&
        channel != SENSOR_CHAN_POWER) { return -ENOTSUP; }
    struct ina260_data *data = dev->data;
    k_mutex_lock(&data->lock, K_FOREVER);
    if (!data->sample.valid) {
        k_mutex_unlock(&data->lock);
        return -ENODATA;
    }
    int64_t micro = channel == SENSOR_CHAN_VOLTAGE ?
        ina260_voltage_micro(data->sample.voltage_raw) :
        channel == SENSOR_CHAN_CURRENT ?
        ina260_current_micro(data->sample.current_raw) :
        ina260_power_micro(data->sample.power_raw);
    int rc = sensor_value_from_micro(value, micro);
    k_mutex_unlock(&data->lock);
    return rc;
}
static int initialize(const struct device *dev)
{
    const struct ina260_config *cfg = dev->config;
    struct ina260_data *data = dev->data;
    k_mutex_init(&data->lock);
    data->sample.valid = false;
    if (!i2c_is_ready_dt(&cfg->bus)) { return -ENODEV; }
    struct ina260_io io = transport(dev);
    /* Startup only identifies the device; first fetch configures it. */
    return ina260_identify(&io);
}
static DEVICE_API(sensor, ina260_api) = {
    .sample_fetch = sample_fetch, .channel_get = channel_get,
};
#define INA260_DEFINE(n) \
    static struct ina260_data data_##n; \
    static const struct ina260_config config_##n = { \
        .bus = I2C_DT_SPEC_INST_GET(n), \
    }; \
    SENSOR_DEVICE_DT_INST_DEFINE(n, initialize, NULL, &data_##n, &config_##n, \
        POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY, &ina260_api);
DT_INST_FOREACH_STATUS_OKAY(INA260_DEFINE)
