/*
 * Fake motion and temperature sensor. One node stands in for the LSM6DSO
 * (accel, gyro, die temperature), the LIS2MDL (magnetometer) or the RP2350 die
 * temperature sensor, serving the quantities its device tree node lists.
 * Each sample re-reads a host file, so a test can play in a tumble or a
 * temperature profile while the flight software runs.
 */
#define DT_DRV_COMPAT proves_sensor_sim

#include <nsi_host_trampolines.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

enum sensor_sim_quantity { SIM_ACCEL, SIM_GYRO, SIM_MAGN, SIM_DIE_TEMP, SIM_QUANTITIES };

static const struct {
    const char* name;
    uint8_t axes;
} sensor_sim_quantities[SIM_QUANTITIES] = {
    [SIM_ACCEL] = {"accel", 3},
    [SIM_GYRO] = {"gyro", 3},
    [SIM_MAGN] = {"magn", 3},
    [SIM_DIE_TEMP] = {"die-temp", 1},
};

/* Values are in micro-units of the Zephyr sensor API: m/s^2, rad/s, gauss, degrees C. */
struct sensor_sim_config {
    const char* sim_file;
    bool has[SIM_QUANTITIES];
    int32_t defaults[SIM_QUANTITIES][3];
};

struct sensor_sim_data {
    int64_t values[SIM_QUANTITIES][3];
};

static void sensor_sim_set_defaults(const struct sensor_sim_config* cfg, struct sensor_sim_data* data) {
    for (int q = 0; q < SIM_QUANTITIES; q++) {
        for (int a = 0; a < 3; a++) {
            data->values[q][a] = cfg->defaults[q][a];
        }
    }
}

/* One "<quantity> <value>..." line, values in SI units (m/s^2, rad/s, gauss, degrees C). */
static void sensor_sim_parse_line(char* line, struct sensor_sim_data* data) {
    for (int q = 0; q < SIM_QUANTITIES; q++) {
        size_t len = strlen(sensor_sim_quantities[q].name);

        if (strncmp(line, sensor_sim_quantities[q].name, len) != 0 || (line[len] != ' ' && line[len] != '\t')) {
            continue;
        }
        char* p = line + len;
        int64_t parsed[3];
        for (int a = 0; a < sensor_sim_quantities[q].axes; a++) {
            char* end;
            double v = strtod(p, &end);

            /* Also rejects NaN and infinity, and keeps the micro-unit value inside int64. */
            if (end == p || !(v > -1e9 && v < 1e9)) {
                return;
            }
            parsed[a] = (int64_t)(v * 1000000.0);
            p = end;
        }
        /* A short or bad line changes nothing, so no reading mixes old and new axes. */
        memcpy(data->values[q], parsed, sensor_sim_quantities[q].axes * sizeof(parsed[0]));
        return;
    }
}

/* No file means the defaults. An empty file, as in the middle of a shell redirect, keeps the last reading. */
static void sensor_sim_read_file(const struct sensor_sim_config* cfg, struct sensor_sim_data* data) {
    char buf[256];
    int fd = nsi_host_open(cfg->sim_file, 0 /* O_RDONLY */);
    long n;

    if (fd < 0) {
        sensor_sim_set_defaults(cfg, data);
        return;
    }
    n = nsi_host_read(fd, buf, sizeof(buf) - 1);
    nsi_host_close(fd);
    if (n <= 0) {
        return;
    }
    buf[n] = '\0';

    sensor_sim_set_defaults(cfg, data);
    for (char* line = buf; line != NULL;) {
        char* next = strchr(line, '\n');

        if (next != NULL) {
            *next++ = '\0';
        }
        sensor_sim_parse_line(line, data);
        line = next;
    }
}

static int sensor_sim_init(const struct device* dev) {
    sensor_sim_set_defaults(dev->config, dev->data);
    return 0;
}

static int sensor_sim_sample_fetch(const struct device* dev, enum sensor_channel chan) {
    ARG_UNUSED(chan);
    sensor_sim_read_file(dev->config, dev->data);
    return 0;
}

static int sensor_sim_channel_get(const struct device* dev, enum sensor_channel chan, struct sensor_value* val) {
    const struct sensor_sim_config* cfg = dev->config;
    struct sensor_sim_data* data = dev->data;
    enum sensor_sim_quantity q;
    int first;
    int count;

    switch (chan) {
        case SENSOR_CHAN_ACCEL_X:
        case SENSOR_CHAN_ACCEL_Y:
        case SENSOR_CHAN_ACCEL_Z:
            q = SIM_ACCEL, first = chan - SENSOR_CHAN_ACCEL_X, count = 1;
            break;
        case SENSOR_CHAN_ACCEL_XYZ:
            q = SIM_ACCEL, first = 0, count = 3;
            break;
        case SENSOR_CHAN_GYRO_X:
        case SENSOR_CHAN_GYRO_Y:
        case SENSOR_CHAN_GYRO_Z:
            q = SIM_GYRO, first = chan - SENSOR_CHAN_GYRO_X, count = 1;
            break;
        case SENSOR_CHAN_GYRO_XYZ:
            q = SIM_GYRO, first = 0, count = 3;
            break;
        case SENSOR_CHAN_MAGN_X:
        case SENSOR_CHAN_MAGN_Y:
        case SENSOR_CHAN_MAGN_Z:
            q = SIM_MAGN, first = chan - SENSOR_CHAN_MAGN_X, count = 1;
            break;
        case SENSOR_CHAN_MAGN_XYZ:
            q = SIM_MAGN, first = 0, count = 3;
            break;
        case SENSOR_CHAN_DIE_TEMP:
            q = SIM_DIE_TEMP, first = 0, count = 1;
            break;
        default:
            return -ENOTSUP;
    }
    if (!cfg->has[q]) {
        return -ENOTSUP;
    }
    for (int a = 0; a < count; a++) {
        sensor_value_from_micro(&val[a], data->values[q][first + a]);
    }
    return 0;
}

/* Accepts the sampling-rate settings ImuManager writes at boot. */
static int sensor_sim_attr_set(const struct device* dev,
                               enum sensor_channel chan,
                               enum sensor_attribute attr,
                               const struct sensor_value* val) {
    ARG_UNUSED(dev);
    ARG_UNUSED(chan);
    ARG_UNUSED(val);
    return attr == SENSOR_ATTR_SAMPLING_FREQUENCY ? 0 : -ENOTSUP;
}

static DEVICE_API(sensor, sensor_sim_api) = {
    .sample_fetch = sensor_sim_sample_fetch,
    .channel_get = sensor_sim_channel_get,
    .attr_set = sensor_sim_attr_set,
};

#define SENSOR_SIM_DEFAULT(n, prop) \
    COND_CODE_1(DT_INST_NODE_HAS_PROP(n, prop), ({DT_INST_FOREACH_PROP_ELEM_SEP(n, prop, DT_PROP_BY_IDX, (, ))}), ({0}))

#define SENSOR_SIM_INIT(n)                                                                                            \
    static const struct sensor_sim_config sensor_sim_config_##n = {                                                   \
        .sim_file = DT_INST_PROP(n, sim_file),                                                                        \
        .has = {DT_INST_NODE_HAS_PROP(n, accel), DT_INST_NODE_HAS_PROP(n, gyro), DT_INST_NODE_HAS_PROP(n, magn),      \
                DT_INST_NODE_HAS_PROP(n, die_temp)},                                                                  \
        .defaults = {SENSOR_SIM_DEFAULT(n, accel), SENSOR_SIM_DEFAULT(n, gyro), SENSOR_SIM_DEFAULT(n, magn),          \
                     SENSOR_SIM_DEFAULT(n, die_temp)},                                                                \
    };                                                                                                                \
    static struct sensor_sim_data sensor_sim_data_##n;                                                                \
    SENSOR_DEVICE_DT_INST_DEFINE(n, sensor_sim_init, NULL, &sensor_sim_data_##n, &sensor_sim_config_##n, POST_KERNEL, \
                                 CONFIG_SENSOR_INIT_PRIORITY, &sensor_sim_api);

DT_INST_FOREACH_STATUS_OKAY(SENSOR_SIM_INIT)
