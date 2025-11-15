// ======================================================================
// \title  Main.cpp
// \brief main program for the F' application. Intended for CLI-based systems (Linux, macOS)
//
// ======================================================================
// Used to access topology functions

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

// Undefine EMPTY macro from Zephyr headers to avoid conflict with F Prime Os::Queue::EMPTY
#ifdef EMPTY
#undef EMPTY
#endif

// #include <FprimeZephyrReference/ReferenceDeployment/Top/ReferenceDeploymentTopology.hpp>

// Deferred-init sensors - will be initialized after face is powered
// const struct device* temp_sens = DEVICE_DT_GET(DT_NODELABEL(face1_temp_sens));

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

    printk("TMP112 Sensor Reading Loop\n");
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
    gpio_pin_set_dt(&face1_enable, 1);
    k_sleep(K_MSEC(200));  // Wait for power to stabilize

    // Get I2C multiplexer and channel devices
    printk("Getting TCA9548A multiplexer and channel...\n");
    const struct device* tca9548a = DEVICE_DT_GET(DT_NODELABEL(tca9548a));
    const struct device* mux_channel_0 = DEVICE_DT_GET(DT_NODELABEL(mux_channel_0));

    if (!device_is_ready(tca9548a)) {
        printk("ERROR: TCA9548A device not ready\n");
        return -1;
    }
    printk("TCA9548A is ready\n");

    if (!device_is_ready(mux_channel_0)) {
        printk("ERROR: Mux channel 0 device not ready\n");
        return -1;
    }
    printk("Mux channel 0 is ready\n\n");

    // Get TMP112 sensor on the mux channel (deferred-init)
    const struct device* mux_temp_sens = DEVICE_DT_GET(DT_NODELABEL(mux_temp_sens));
    
    // Initialize the TMP112 sensor (deferred-init requires explicit init)
    printk("Initializing TMP112 on mux channel...\n");
    int ret_1 = device_init(mux_temp_sens);
    if (ret_1 < 0) {
        printk("ERROR: Failed to initialize TMP112 (error %d)\n", ret_1);
        return -1;
    }
    
    if (!device_is_ready(mux_temp_sens)) {
        printk("ERROR: TMP112 sensor on mux channel not ready after init\n");
        return -1;
    }
    printk("TMP112 sensor initialized and ready\n\n");

    // Get VEML6031 sensor on the mux channel (deferred-init)
    const struct device* mux_light_sens = DEVICE_DT_GET(DT_NODELABEL(mux_light_sens));
    
    // Initialize the VEML6031 sensor (deferred-init requires explicit init)
    printk("Initializing VEML6031 on mux channel...\n");
    int ret_2 = device_init(mux_light_sens);
    if (ret_2 < 0) {
        printk("ERROR: Failed to initialize VEML6031 (error %d)\n", ret_2);
        return -1;
    }
    
    if (!device_is_ready(mux_light_sens)) {
        printk("ERROR: VEML6031 sensor on mux channel not ready after init\n");
        return -1;
    }
    printk("VEML6031 sensor initialized and ready\n\n");

    // I2C address scanning function
    printk("Starting I2C address scan on TCA9548A channel 0...\n");
    printk("Scanning addresses 0x03 to 0x77...\n\n");

    int found_count = 0;
    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        uint8_t dummy;
        
        // Try to read 1 byte from the address
        int ret = i2c_read(mux_channel_0, &dummy, 1, addr);
        
        if (ret == 0) {
            printk("  Device found at address 0x%02X\n", addr);
            found_count++;
        }
        
        // Small delay between scans
        k_sleep(K_MSEC(1));
    }

    printk("\nScan complete. Found %d device(s)\n\n", found_count);

    // Read temperature from TMP112
    printk("Reading temperature from TMP112...\n");
    
    int ret = sensor_sample_fetch(mux_temp_sens);
    if (ret < 0) {
        printk("ERROR: Failed to fetch TMP112 sample (error %d)\n", ret);
    } else {
        struct sensor_value temperature;
        ret = sensor_channel_get(mux_temp_sens, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
        if (ret < 0) {
            printk("ERROR: Failed to get temperature channel (error %d)\n", ret);
        } else {
            double temp_celsius = sensor_value_to_double(&temperature);
            printk("Temperature: %.2f °C\n\n", temp_celsius);
        }
    }

    // Read light from VEML6031
    printk("Reading light from VEML6031...\n");
    
    ret = sensor_sample_fetch(mux_light_sens);
    if (ret == -E2BIG) {
        printk("Light: OVERFLOW (>bright limit)\n");
    } else if (ret < 0) {
        printk("ERROR: Failed to fetch VEML6031 sample (error %d)\n", ret);
    } else {
        struct sensor_value light;
        ret = sensor_channel_get(mux_light_sens, SENSOR_CHAN_LIGHT, &light);
        if (ret < 0) {
            printk("ERROR: Failed to get light channel (error %d)\n", ret);
        } else {
            double light_lux = sensor_value_to_double(&light);
            printk("Light: %.2f lux\n\n", light_lux);
        }
    }

    // Loop forever - read sensors every 5 seconds
    while (1) {
        k_sleep(K_MSEC(1000));
        
        printk("\n--- Sensor Readings ---\n");
        
        // Temperature
        ret = sensor_sample_fetch(mux_temp_sens);
        if (ret < 0) {
            printk("ERROR: Failed to fetch TMP112 sample (error %d)\n", ret);
        } else {
            struct sensor_value temperature;
            ret = sensor_channel_get(mux_temp_sens, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
            if (ret < 0) {
                printk("ERROR: Failed to get temperature channel (error %d)\n", ret);
            } else {
                double temp_celsius = sensor_value_to_double(&temperature);
                printk("Temperature: %.2f °C\n", temp_celsius);
            }
        }
        
        // Light
        ret = sensor_sample_fetch(mux_light_sens);
        if (ret == -E2BIG) {
            printk("Light: OVERFLOW (>bright limit)\n");
        } else if (ret < 0) {
            printk("ERROR: Failed to fetch VEML6031 sample (error %d)\n", ret);
        } else {
            struct sensor_value light;
            ret = sensor_channel_get(mux_light_sens, SENSOR_CHAN_LIGHT, &light);
            if (ret < 0) {
                printk("ERROR: Failed to get light channel (error %d)\n", ret);
            } else {
                double light_lux = sensor_value_to_double(&light);
                printk("Light: %.2f lux\n", light_lux);
            }
        }
    }

    return 0;
}
