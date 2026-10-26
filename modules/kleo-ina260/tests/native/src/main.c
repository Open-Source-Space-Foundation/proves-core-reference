/* SPDX-License-Identifier: Apache-2.0 */
#define DT_DRV_COMPAT ti_ina260
#include <errno.h>
#include <string.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c_emul.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/ztest.h>

struct fake_sensor {
    uint16_t regs[256];
    int fail_reg;
    bool fail_write;
    unsigned transactions;
};
static void reset(struct fake_sensor *s)
{
    memset(s, 0, sizeof(*s));
    s->fail_reg = -1;
    s->regs[0xfe] = 0x5449;
    s->regs[0xff] = 0x2271;
    s->regs[6] = 8;
    s->regs[1] = 0xffff;
    s->regs[2] = 4000;
    s->regs[3] = 123;
}
static int transfer(const struct emul *em, struct i2c_msg *msg, int count, int addr)
{
    struct fake_sensor *s = em->data;
    ARG_UNUSED(addr);
    s->transactions++;
    if (count == 2 && msg[0].len == 1 && !(msg[0].flags & I2C_MSG_READ) &&
        msg[1].len == 2 && (msg[1].flags & I2C_MSG_READ)) {
        uint8_t reg = msg[0].buf[0];
        if (s->fail_reg == reg) { return -EIO; }
        msg[1].buf[0] = s->regs[reg] >> 8;
        msg[1].buf[1] = s->regs[reg] & 0xff;
        return 0;
    }
    if (count == 1 && msg[0].len == 3 && !(msg[0].flags & I2C_MSG_READ)) {
        if (s->fail_write) { return -EIO; }
        /* Reject unexpected writes, including INA219 calibration. */
        if (msg[0].buf[0] != 0 || msg[0].buf[1] != 0x61 ||
            msg[0].buf[2] != 0x23) { return -EINVAL; }
        s->regs[0] = 0x6123;
        return 0;
    }
    return -EIO;
}
static int init(const struct emul *em, const struct device *parent)
{
    ARG_UNUSED(parent);
    reset(em->data);
    return 0;
}
static const struct i2c_emul_api bus_api = { .transfer = transfer };
#define FAKE_DEFINE(n) \
    static struct fake_sensor fake_##n; \
    EMUL_DT_INST_DEFINE(n, init, &fake_##n, NULL, &bus_api, NULL);
DT_INST_FOREACH_STATUS_OKAY(FAKE_DEFINE)

static const struct device *const a = DEVICE_DT_GET(DT_NODELABEL(monitor_a));
static const struct device *const b = DEVICE_DT_GET(DT_NODELABEL(monitor_b));
static struct fake_sensor *state_a(void)
{
    return EMUL_DT_GET(DT_NODELABEL(monitor_a))->data;
}
static void before(void *fixture)
{
    ARG_UNUSED(fixture);
    zassert_true(device_is_ready(a));
    zassert_true(device_is_ready(b));
    reset(state_a());
    reset(EMUL_DT_GET(DT_NODELABEL(monitor_b))->data);
    /* Invalidate any previous cached sample through the public driver API. */
    state_a()->fail_reg = 0xfe;
    zassert_equal(sensor_sample_fetch(a), -EIO);
    state_a()->fail_reg = -1;
}
ZTEST(ina260, test_values_and_cached_reads)
{
    struct sensor_value value;
    zassert_equal(sensor_channel_get(a, SENSOR_CHAN_CURRENT, &value), -ENODATA);
    zassert_ok(sensor_sample_fetch(a));
    unsigned count = state_a()->transactions;
    zassert_ok(sensor_channel_get(a, SENSOR_CHAN_CURRENT, &value));
    zassert_equal(value.val1, 0);
    zassert_equal(value.val2, -1250);
    zassert_ok(sensor_channel_get(a, SENSOR_CHAN_VOLTAGE, &value));
    zassert_equal(value.val1, 5);
    zassert_equal(value.val2, 0);
    zassert_ok(sensor_channel_get(a, SENSOR_CHAN_POWER, &value));
    zassert_equal(value.val1, 1);
    zassert_equal(value.val2, 230000);
    zassert_equal(state_a()->transactions, count);
}
ZTEST(ina260, test_failure_invalidates_and_recovers)
{
    struct sensor_value value;
    zassert_ok(sensor_sample_fetch(a));
    state_a()->fail_reg = 3;
    zassert_equal(sensor_sample_fetch(a), -EIO);
    zassert_equal(sensor_channel_get(a, SENSOR_CHAN_VOLTAGE, &value), -ENODATA);
    zassert_equal(sensor_channel_get(a, SENSOR_CHAN_CURRENT, &value), -ENODATA);
    zassert_equal(sensor_channel_get(a, SENSOR_CHAN_POWER, &value), -ENODATA);
    state_a()->fail_reg = -1;
    zassert_ok(sensor_sample_fetch(a));
    state_a()->fail_write = true;
    zassert_equal(sensor_sample_fetch(a), -EIO);
}
ZTEST(ina260, test_timeout_and_overflow)
{
    state_a()->regs[6] = 0;
    zassert_equal(sensor_sample_fetch(a), -ETIMEDOUT);
    state_a()->regs[6] = 12;
    zassert_equal(sensor_sample_fetch(a), -ERANGE);
    state_a()->regs[6] = 8;
    zassert_ok(sensor_sample_fetch(a));
}
ZTEST(ina260, test_identity_and_unsupported_channel)
{
    struct sensor_value value;
    unsigned count = state_a()->transactions;
    zassert_equal(sensor_sample_fetch_chan(a, SENSOR_CHAN_AMBIENT_TEMP), -ENOTSUP);
    zassert_equal(sensor_channel_get(a, SENSOR_CHAN_AMBIENT_TEMP, &value), -ENOTSUP);
    zassert_equal(state_a()->transactions, count);
    state_a()->regs[0xff] = 0x2190;
    zassert_equal(sensor_sample_fetch(a), -ENODEV);
}
ZTEST(ina260, test_independent_devices)
{
    struct sensor_value value;
    state_a()->regs[2] = 8000;
    zassert_ok(sensor_sample_fetch(a));
    zassert_ok(sensor_sample_fetch(b));
    zassert_ok(sensor_channel_get(a, SENSOR_CHAN_VOLTAGE, &value));
    zassert_equal(value.val1, 10);
    zassert_ok(sensor_channel_get(b, SENSOR_CHAN_VOLTAGE, &value));
    zassert_equal(value.val1, 5);
}
ZTEST_SUITE(ina260, NULL, NULL, before, NULL, NULL);
