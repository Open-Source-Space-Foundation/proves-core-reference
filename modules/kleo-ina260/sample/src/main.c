/* Build/test candidate only: inherited board startup and deployment need review. */
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

static void report(const struct device *dev, unsigned int address)
{
    if (!device_is_ready(dev)) {
        printk("0x%02x: initialization failed; no valid readings\n", address);
        return;
    }
    struct sensor_value v, i, p;
    int rc = sensor_sample_fetch(dev);
    if (rc == 0) { rc = sensor_channel_get(dev, SENSOR_CHAN_VOLTAGE, &v); }
    if (rc == 0) { rc = sensor_channel_get(dev, SENSOR_CHAN_CURRENT, &i); }
    if (rc == 0) { rc = sensor_channel_get(dev, SENSOR_CHAN_POWER, &p); }
    if (rc != 0) {
        printk("0x%02x: sample INVALID, error=%d\n", address, rc);
        return;
    }
    printk("0x%02x: voltage=%lld uV current=%lld uA power=%lld uW\n",
           address, (long long)v.val1 * 1000000 + v.val2,
           (long long)i.val1 * 1000000 + i.val2,
           (long long)p.val1 * 1000000 + p.val2);
}
int main(void)
{
    k_sleep(K_SECONDS(2));
    printk("KLEO INA260 sample: triggered reads, I2C1 GPIO2/3\n");
    /* One owner for fetch + get prevents interleaving across consumers. */
    for (int n = 0; n < 10; ++n) {
        report(DEVICE_DT_GET(DT_NODELABEL(ina260_40)), 0x40);
        report(DEVICE_DT_GET(DT_NODELABEL(ina260_41)), 0x41);
        k_sleep(K_SECONDS(1));
    }
    return 0;
}
