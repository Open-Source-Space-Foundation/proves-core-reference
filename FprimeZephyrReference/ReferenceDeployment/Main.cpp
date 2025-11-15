// ======================================================================
// \title  Main.cpp
// \brief main program for the F' application. Intended for CLI-based systems (Linux, macOS)
//
// ======================================================================
// Used to access topology functions

#include <FprimeZephyrReference/ReferenceDeployment/Top/ReferenceDeploymentTopology.hpp>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

// Deferred-init sensors - will be initialized after face is powered
const struct device* temp_sens = DEVICE_DT_GET(DT_NODELABEL(temp_sens));
const struct device* light_sens = DEVICE_DT_GET(DT_NODELABEL(light_sens));

// Face enable pins from MCP23017 GPIO expander
static const struct gpio_dt_spec face0_enable = GPIO_DT_SPEC_GET(DT_NODELABEL(face0_enable), gpios);
static const struct gpio_dt_spec face1_enable = GPIO_DT_SPEC_GET(DT_NODELABEL(face1_enable), gpios);
static const struct gpio_dt_spec face2_enable = GPIO_DT_SPEC_GET(DT_NODELABEL(face2_enable), gpios);
static const struct gpio_dt_spec face3_enable = GPIO_DT_SPEC_GET(DT_NODELABEL(face3_enable), gpios);

int main(int argc, char* argv[]) {
    // ** DO NOT REMOVE **//
    //
    // This sleep is necessary to allow the USB CDC ACM interface to initialize before
    // the application starts writing to it.
    k_sleep(K_MSEC(3000));

    printk("TMP112 & VEML6031 Sensor Reading Loop\n");
    printk("======================================\n\n");

    // Initialize Face Enable GPIO pins
    printk("Initializing Face Enable pins...\n");

    if (!gpio_is_ready_dt(&face0_enable)) {
        printk("ERROR: Face0 enable GPIO not ready\n");
        return -1;
    }
    if (gpio_pin_configure_dt(&face0_enable, GPIO_OUTPUT_INACTIVE) < 0) {
        printk("ERROR: Failed to configure Face0 enable pin\n");
        return -1;
    }
    printk("Face0 enable pin configured (LOW/DISABLED)\n");

    if (!gpio_is_ready_dt(&face1_enable)) {
        printk("ERROR: Face1 enable GPIO not ready\n");
        return -1;
    }
    if (gpio_pin_configure_dt(&face1_enable, GPIO_OUTPUT_INACTIVE) < 0) {
        printk("ERROR: Failed to configure Face1 enable pin\n");
        return -1;
    }
    printk("Face1 enable pin configured (LOW/DISABLED)\n");

    if (!gpio_is_ready_dt(&face2_enable)) {
        printk("ERROR: Face2 enable GPIO not ready\n");
        return -1;
    }
    if (gpio_pin_configure_dt(&face2_enable, GPIO_OUTPUT_INACTIVE) < 0) {
        printk("ERROR: Failed to configure Face2 enable pin\n");
        return -1;
    }
    printk("Face2 enable pin configured (LOW/DISABLED)\n");

    if (!gpio_is_ready_dt(&face3_enable)) {
        printk("ERROR: Face3 enable GPIO not ready\n");
        return -1;
    }
    if (gpio_pin_configure_dt(&face3_enable, GPIO_OUTPUT_INACTIVE) < 0) {
        printk("ERROR: Failed to configure Face3 enable pin\n");
        return -1;
    }
    printk("Face3 enable pin configured (LOW/DISABLED)\n\n");

    // Enable Face0 temporarily for sensor initialization
    printk("Enabling Face0 for sensor initialization...\n");
    gpio_pin_set_dt(&face0_enable, 1);
    k_sleep(K_MSEC(200));  // Wait for power to stabilize

    // Initialize the deferred-init sensors
    printk("Initializing TMP112...\n");
    int ret = device_init(temp_sens);
    if (ret < 0) {
        printk("ERROR: Failed to initialize TMP112 (error %d)\n", ret);
        return -1;
    }

    printk("Initializing VEML6031...\n");
    ret = device_init(light_sens);
    if (ret < 0) {
        printk("ERROR: Failed to initialize VEML6031 (error %d)\n", ret);
        return -1;
    }

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
    ret = sensor_attr_set(light_sens, SENSOR_CHAN_LIGHT, SENSOR_ATTR_CONFIGURATION, &it_val);
    if (ret < 0) {
        printk("Warning: Failed to set integration time (error %d)\n", ret);
    }

    // Set gain to lowest (1x) to handle bright light
    struct sensor_value gain_val;
    gain_val.val1 = 0;  // Gain = 1x (lowest, widest range)
    gain_val.val2 = 0;
    ret = sensor_attr_set(light_sens, SENSOR_CHAN_LIGHT, SENSOR_ATTR_GAIN, &gain_val);
    if (ret < 0) {
        printk("Warning: Failed to set gain (error %d)\n", ret);
    }

    printk("VEML6031 configured for wide dynamic range\n");

    // Disable Face0 after initialization
    gpio_pin_set_dt(&face0_enable, 0);
    printk("Face0 disabled after initialization\n\n");

    // Array of face enable pins for cycling
    const struct gpio_dt_spec* face_enables[] = {&face0_enable, &face1_enable, &face2_enable, &face3_enable};
    const char* face_names[] = {"Face0", "Face1", "Face2", "Face3"};
    int current_face = 0;

    // Reading loop - cycle through faces
    while (1) {
        struct sensor_value temperature;
        struct sensor_value light;
        int ret;

        // Enable current face
        gpio_pin_set_dt(face_enables[current_face], 1);
        printk("\n=== %s ENABLED ===\n", face_names[current_face]);

        // Wait for sensors to stabilize after power-on
        k_sleep(K_MSEC(100));

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

        // Disable current face
        gpio_pin_set_dt(face_enables[current_face], 0);
        printk("%s DISABLED\n", face_names[current_face]);

        // Move to next face
        current_face = (current_face + 1) % 4;

        // Wait before next face
        k_sleep(K_MSEC(500));
    }

    return 0;
}
