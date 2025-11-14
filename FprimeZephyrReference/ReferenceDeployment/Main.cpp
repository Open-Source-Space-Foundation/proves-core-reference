// ======================================================================
// \title  Main.cpp
// \brief main program for the F' application. Intended for CLI-based systems (Linux, macOS)
//
// ======================================================================
// Used to access topology functions

#include <FprimeZephyrReference/ReferenceDeployment/Top/ReferenceDeploymentTopology.hpp>

#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

const struct device* temp_sens = DEVICE_DT_GET(DT_NODELABEL(temp_sens));
const struct device* light_sens = DEVICE_DT_GET(DT_NODELABEL(light_sens));

int main(int argc, char* argv[]) {
    // ** DO NOT REMOVE **//
    //
    // This sleep is necessary to allow the USB CDC ACM interface to initialize before
    // the application starts writing to it.
    k_sleep(K_MSEC(3000));

    printk("TMP112 & VEML6031 Sensor Reading Loop\n");
    printk("======================================\n\n");

    // Check if TMP112 device is ready
    if (!device_is_ready(temp_sens)) {
        printk("ERROR: TMP112 device is not ready\n");
        return -1;
    }
    printk("TMP112 device is ready at %p\n", temp_sens);

    // Check if VEML6031 device is ready
    if (!device_is_ready(light_sens)) {
        printk("ERROR: VEML6031 device is not ready\n");
        return -1;
    }
    printk("VEML6031 device is ready at %p\n", light_sens);

    // Configure VEML6031 for wider dynamic range
    // Set integration time to lowest (25ms) for faster response and less overflow
    struct sensor_value it_val;
    it_val.val1 = 0;  // IT = 25ms (fastest, least sensitive)
    it_val.val2 = 0;
    ret = sensor_attr_set(light_sens, SENSOR_CHAN_LIGHT, (enum sensor_attribute)SENSOR_ATTR_VEML6031_IT, &it_val);
    if (ret < 0) {
        printk("Warning: Failed to set integration time (error %d)\n", ret);
    }

    // Set gain to lowest (1x) to handle bright light
    struct sensor_value gain_val;
    gain_val.val1 = 0;  // Gain = 1x (lowest, widest range)
    gain_val.val2 = 0;
    ret = sensor_attr_set(light_sens, SENSOR_CHAN_LIGHT, (enum sensor_attribute)SENSOR_ATTR_VEML6031_GAIN, &gain_val);
    if (ret < 0) {
        printk("Warning: Failed to set gain (error %d)\n", ret);
    }

    printk("VEML6031 configured for wide dynamic range\n\n");

    // Reading loop
    while (1) {
        struct sensor_value temperature;
        struct sensor_value light;
        int ret;

        // === TMP112 Temperature Sensor ===
        ret = sensor_sample_fetch(temp_sens);
        if (ret < 0) {
            printk("ERROR: Failed to fetch TMP112 sample (error %d)\n", ret);
        } else {
            ret = sensor_channel_get(temp_sens, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
            if (ret < 0) {
                printk("ERROR: Failed to get temperature channel (error %d)\n", ret);
            } else {
                double temp_celsius = sensor_value_to_double(&temperature);
                printk("Temperature: %.2f °C | ", temp_celsius);
            }
        }

        // === VEML6031 Light Sensor ===
        ret = sensor_sample_fetch(light_sens);
        if (ret == -E2BIG) {
            // Sensor overflowed - reading too bright
            printk("Light: OVERFLOW (>bright limit)\n");
        } else if (ret < 0) {
            printk("ERROR: Failed to fetch VEML6031 sample (error %d)\n", ret);
        } else {
            ret = sensor_channel_get(light_sens, SENSOR_CHAN_LIGHT, &light);
            if (ret < 0) {
                printk("ERROR: Failed to get light channel (error %d)\n", ret);
            } else {
                double light_lux = sensor_value_to_double(&light);
                printk("Light: %.2f lux\n", light_lux);
            }
        }

        // Wait 1 second before next reading
        k_sleep(K_MSEC(1000));
    }

    return 0;
}
