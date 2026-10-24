/*
 * Fake INA219 serving the three channels Ina219Manager reads. Each sample
 * re-reads a host file, so a test can change the battery voltage while the
 * flight software runs. A restart would not do: the flight software treats it
 * as an unintended reboot and enters safe mode.
 */
#define DT_DRV_COMPAT proves_ina219_sim

#include <nsi_host_trampolines.h>
#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

struct ina219_sim_config {
    const char* sim_file;
    int32_t default_mv;
    int32_t default_ma;
};

struct ina219_sim_data {
    int32_t mv;
    int32_t ma;
};

/* No file means the defaults. An empty file, as in the middle of a shell redirect, keeps the last reading. */
static void ina219_sim_read_file(const struct ina219_sim_config* cfg, int32_t* mv, int32_t* ma) {
    char buf[32];
    char* end;
    int fd = nsi_host_open(cfg->sim_file, 0 /* O_RDONLY */);
    long n;

    if (fd < 0) {
        *mv = cfg->default_mv;
        *ma = cfg->default_ma;
        return;
    }
    n = nsi_host_read(fd, buf, sizeof(buf) - 1);
    nsi_host_close(fd);
    if (n <= 0) {
        return;
    }
    buf[n] = '\0';

    long v = strtol(buf, &end, 10);

    if (end == buf) {
        return;
    }
    *mv = v;

    char* rest = end;
    long a = strtol(rest, &end, 10);

    *ma = (end != rest) ? a : cfg->default_ma;
}

static int ina219_sim_sample_fetch(const struct device* dev, enum sensor_channel chan) {
    const struct ina219_sim_config* cfg = dev->config;
    struct ina219_sim_data* data = dev->data;

    ARG_UNUSED(chan);
    ina219_sim_read_file(cfg, &data->mv, &data->ma);
    return 0;
}

static int ina219_sim_channel_get(const struct device* dev, enum sensor_channel chan, struct sensor_value* val) {
    struct ina219_sim_data* data = dev->data;

    switch (chan) {
        case SENSOR_CHAN_VOLTAGE:
            return sensor_value_from_milli(val, data->mv);
        case SENSOR_CHAN_CURRENT:
            return sensor_value_from_milli(val, data->ma);
        case SENSOR_CHAN_POWER:
            return sensor_value_from_micro(val, (int64_t)data->mv * data->ma);
        default:
            return -ENOTSUP;
    }
}

static DEVICE_API(sensor, ina219_sim_api) = {
    .sample_fetch = ina219_sim_sample_fetch,
    .channel_get = ina219_sim_channel_get,
};

#define INA219_SIM_INIT(n)                                                                                 \
    static const struct ina219_sim_config ina219_sim_config_##n = {                                        \
        .sim_file = DT_INST_PROP(n, sim_file),                                                             \
        .default_mv = DT_INST_PROP(n, default_millivolts),                                                 \
        .default_ma = DT_INST_PROP(n, default_milliamps),                                                  \
    };                                                                                                     \
    static struct ina219_sim_data ina219_sim_data_##n = {                                                  \
        .mv = DT_INST_PROP(n, default_millivolts),                                                         \
        .ma = DT_INST_PROP(n, default_milliamps),                                                          \
    };                                                                                                     \
    SENSOR_DEVICE_DT_INST_DEFINE(n, NULL, NULL, &ina219_sim_data_##n, &ina219_sim_config_##n, POST_KERNEL, \
                                 CONFIG_SENSOR_INIT_PRIORITY, &ina219_sim_api);

DT_INST_FOREACH_STATUS_OKAY(INA219_SIM_INIT)
