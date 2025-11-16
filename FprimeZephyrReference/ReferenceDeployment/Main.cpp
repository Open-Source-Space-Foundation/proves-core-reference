// ======================================================================
// \title  Main.cpp
// \brief main program for the F' application. Intended for CLI-based systems (Linux, macOS)
//
// ======================================================================
// Used to access topology functions

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/haptics.h>
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

    // Get I2C multiplexer and channel devices
    printk("Getting TCA9548A multiplexer and channels...\n");
    const struct device* tca9548a = DEVICE_DT_GET(DT_NODELABEL(tca9548a));
    const struct device* mux_channel_0 = DEVICE_DT_GET(DT_NODELABEL(mux_channel_0));
    const struct device* mux_channel_1 = DEVICE_DT_GET(DT_NODELABEL(mux_channel_1));

    if (!device_is_ready(tca9548a)) {
        printk("ERROR: TCA9548A device not ready\n");
        return -1;
    }
    printk("TCA9548A is ready\n");

    if (!device_is_ready(mux_channel_0)) {
        printk("ERROR: Mux channel 0 device not ready\n");
        return -1;
    }
    printk("Mux channel 0 is ready\n");

    if (!device_is_ready(mux_channel_1)) {
        printk("ERROR: Mux channel 1 device not ready\n");
        return -1;
    }
    printk("Mux channel 1 is ready\n\n");

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

    // Enable Face1 temporarily for sensor initialization
    printk("Enabling Face0 and Face1 for sensor initialization...\n");
    gpio_pin_set_dt(&face0_enable, 1);
    gpio_pin_set_dt(&face1_enable, 1);
    k_sleep(K_MSEC(200));  // Wait for power to stabilize

    // ========================================
    // Initialize Mux Channel 0 Sensors
    // ========================================
    printk("\n--- Initializing Mux Channel 0 Sensors ---\n");

    // Get TMP112 sensor on mux channel 0 (deferred-init)
    const struct device* mux0_temp_sens = DEVICE_DT_GET(DT_NODELABEL(mux0_temp_sens));
    
    // Initialize the TMP112 sensor (deferred-init requires explicit init)
    printk("Initializing TMP112 on mux channel 0...\n");
    int ret_1 = device_init(mux0_temp_sens);
    if (ret_1 < 0) {
        printk("ERROR: Failed to initialize TMP112 on channel 0 (error %d)\n", ret_1);
        return -1;
    }
    
    if (!device_is_ready(mux0_temp_sens)) {
        printk("ERROR: TMP112 sensor on mux channel 0 not ready after init\n");
        return -1;
    }
    printk("TMP112 sensor on channel 0 initialized and ready\n");

    // Get VEML6031 sensor on mux channel 0 (deferred-init)
    const struct device* mux0_light_sens = DEVICE_DT_GET(DT_NODELABEL(mux0_light_sens));
    
    // Initialize the VEML6031 sensor (deferred-init requires explicit init)
    printk("Initializing VEML6031 on mux channel 0...\n");
    int ret_2 = device_init(mux0_light_sens);
    if (ret_2 < 0) {
        printk("ERROR: Failed to initialize VEML6031 on channel 0 (error %d)\n", ret_2);
        return -1;
    }
    
    if (!device_is_ready(mux0_light_sens)) {
        printk("ERROR: VEML6031 sensor on mux channel 0 not ready after init\n");
        return -1;
    }
    printk("VEML6031 sensor on channel 0 initialized and ready\n");

    // Get DRV2605 haptic driver on mux channel 0 (deferred-init)
    const struct device* mux0_drv2605 = DEVICE_DT_GET(DT_NODELABEL(mux0_drv2605));
    
    // Initialize the DRV2605 (deferred-init requires explicit init)
    printk("Initializing DRV2605 on mux channel 0...\n");
    int ret_3 = device_init(mux0_drv2605);
    if (ret_3 < 0) {
        printk("ERROR: Failed to initialize DRV2605 on channel 0 (error %d)\n", ret_3);
        return -1;
    }
    printk("DRV2605 on channel 0 initialized and ready\n");

    // ========================================
    // Initialize Mux Channel 1 Sensors
    // ========================================
    printk("\n--- Initializing Mux Channel 1 Sensors ---\n");

    // Get TMP112 sensor on mux channel 1 (deferred-init)
    const struct device* mux1_temp_sens = DEVICE_DT_GET(DT_NODELABEL(mux1_temp_sens));
    
    // Initialize the TMP112 sensor (deferred-init requires explicit init)
    printk("Initializing TMP112 on mux channel 1...\n");
    int ret_4 = device_init(mux1_temp_sens);
    if (ret_4 < 0) {
        printk("ERROR: Failed to initialize TMP112 on channel 1 (error %d)\n", ret_4);
        return -1;
    }
    
    if (!device_is_ready(mux1_temp_sens)) {
        printk("ERROR: TMP112 sensor on mux channel 1 not ready after init\n");
        return -1;
    }
    printk("TMP112 sensor on channel 1 initialized and ready\n");

    // Get VEML6031 sensor on mux channel 1 (deferred-init)
    const struct device* mux1_light_sens = DEVICE_DT_GET(DT_NODELABEL(mux1_light_sens));
    
    // Initialize the VEML6031 sensor (deferred-init requires explicit init)
    printk("Initializing VEML6031 on mux channel 1...\n");
    int ret_5 = device_init(mux1_light_sens);
    if (ret_5 < 0) {
        printk("ERROR: Failed to initialize VEML6031 on channel 1 (error %d)\n", ret_5);
        return -1;
    }
    
    if (!device_is_ready(mux1_light_sens)) {
        printk("ERROR: VEML6031 sensor on mux channel 1 not ready after init\n");
        return -1;
    }
    printk("VEML6031 sensor on channel 1 initialized and ready\n");

    // Get DRV2605 haptic driver on mux channel 1 (deferred-init)
    const struct device* mux1_drv2605 = DEVICE_DT_GET(DT_NODELABEL(mux1_drv2605));
    
    // Initialize the DRV2605 (deferred-init requires explicit init)
    printk("Initializing DRV2605 on mux channel 1...\n");
    int ret_6 = device_init(mux1_drv2605);
    if (ret_6 < 0) {
        printk("ERROR: Failed to initialize DRV2605 on channel 1 (error %d)\n", ret_6);
        return -1;
    }
    printk("DRV2605 on channel 1 initialized and ready\n\n");

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

    // Read initial temperature from both channels
    printk("--- Initial Sensor Readings ---\n");
    printk("\nMux Channel 0:\n");
    
    int ret = sensor_sample_fetch(mux0_temp_sens);
    if (ret < 0) {
        printk("ERROR: Failed to fetch TMP112 sample from channel 0 (error %d)\n", ret);
    } else {
        struct sensor_value temperature;
        ret = sensor_channel_get(mux0_temp_sens, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
        if (ret < 0) {
            printk("ERROR: Failed to get temperature channel from channel 0 (error %d)\n", ret);
        } else {
            double temp_celsius = sensor_value_to_double(&temperature);
            printk("Temperature: %.2f °C\n", temp_celsius);
        }
    }

    ret = sensor_sample_fetch(mux0_light_sens);
    if (ret == -E2BIG) {
        printk("Light: OVERFLOW (>bright limit)\n");
    } else if (ret < 0) {
        printk("ERROR: Failed to fetch VEML6031 sample from channel 0 (error %d)\n", ret);
    } else {
        struct sensor_value light;
        ret = sensor_channel_get(mux0_light_sens, SENSOR_CHAN_LIGHT, &light);
        if (ret < 0) {
            printk("ERROR: Failed to get light channel from channel 0 (error %d)\n", ret);
        } else {
            double light_lux = sensor_value_to_double(&light);
            printk("Light: %.2f lux\n", light_lux);
        }
    }

    printk("\nMux Channel 1:\n");
    
    ret = sensor_sample_fetch(mux1_temp_sens);
    if (ret < 0) {
        printk("ERROR: Failed to fetch TMP112 sample from channel 1 (error %d)\n", ret);
    } else {
        struct sensor_value temperature;
        ret = sensor_channel_get(mux1_temp_sens, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
        if (ret < 0) {
            printk("ERROR: Failed to get temperature channel from channel 1 (error %d)\n", ret);
        } else {
            double temp_celsius = sensor_value_to_double(&temperature);
            printk("Temperature: %.2f °C\n", temp_celsius);
        }
    }

    ret = sensor_sample_fetch(mux1_light_sens);
    if (ret == -E2BIG) {
        printk("Light: OVERFLOW (>bright limit)\n\n");
    } else if (ret < 0) {
        printk("ERROR: Failed to fetch VEML6031 sample from channel 1 (error %d)\n", ret);
    } else {
        struct sensor_value light;
        ret = sensor_channel_get(mux1_light_sens, SENSOR_CHAN_LIGHT, &light);
        if (ret < 0) {
            printk("ERROR: Failed to get light channel from channel 1 (error %d)\n", ret);
        } else {
            double light_lux = sensor_value_to_double(&light);
            printk("Light: %.2f lux\n\n", light_lux);
        }
    }

    // Loop forever - read sensors every second
    while (1) {
        k_sleep(K_MSEC(1000));
        
        printk("\n======================================\n");
        printk("--- Sensor Readings ---\n");
        printk("======================================\n");
        
        // ========================================
        // Mux Channel 0 Readings
        // ========================================
        printk("\nMux Channel 0:\n");
        
        // Temperature
        ret = sensor_sample_fetch(mux0_temp_sens);
        if (ret < 0) {
            printk("ERROR: Failed to fetch TMP112 sample from channel 0 (error %d)\n", ret);
        } else {
            struct sensor_value temperature;
            ret = sensor_channel_get(mux0_temp_sens, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
            if (ret < 0) {
                printk("ERROR: Failed to get temperature channel from channel 0 (error %d)\n", ret);
            } else {
                double temp_celsius = sensor_value_to_double(&temperature);
                printk("Temperature: %.2f °C\n", temp_celsius);
            }
        }
        
        // Light
        ret = sensor_sample_fetch(mux0_light_sens);
        if (ret == -E2BIG) {
            printk("Light: OVERFLOW (>bright limit)\n");
        } else if (ret < 0) {
            printk("ERROR: Failed to fetch VEML6031 sample from channel 0 (error %d)\n", ret);
        } else {
            struct sensor_value light;
            ret = sensor_channel_get(mux0_light_sens, SENSOR_CHAN_LIGHT, &light);
            if (ret < 0) {
                printk("ERROR: Failed to get light channel from channel 0 (error %d)\n", ret);
            } else {
                double light_lux = sensor_value_to_double(&light);
                printk("Light: %.2f lux\n", light_lux);
            }
        }
        
        // Haptic - trigger a short vibration
        printk("Triggering haptic on channel 0...\n");
        ret = haptics_start_output(mux0_drv2605);
        if (ret < 0) {
            printk("ERROR: Failed to start haptic on channel 0 (error %d)\n", ret);
        } else {
            printk("Haptic: Started\n");
            k_sleep(K_MSEC(100));  // Vibrate for 100ms
            
            ret = haptics_stop_output(mux0_drv2605);
            if (ret < 0) {
                printk("ERROR: Failed to stop haptic on channel 0 (error %d)\n", ret);
            } else {
                printk("Haptic: Stopped\n");
            }
        }

        // ========================================
        // Mux Channel 1 Readings
        // ========================================
        printk("\nMux Channel 1:\n");
        
        // Temperature
        ret = sensor_sample_fetch(mux1_temp_sens);
        if (ret < 0) {
            printk("ERROR: Failed to fetch TMP112 sample from channel 1 (error %d)\n", ret);
        } else {
            struct sensor_value temperature;
            ret = sensor_channel_get(mux1_temp_sens, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
            if (ret < 0) {
                printk("ERROR: Failed to get temperature channel from channel 1 (error %d)\n", ret);
            } else {
                double temp_celsius = sensor_value_to_double(&temperature);
                printk("Temperature: %.2f °C\n", temp_celsius);
            }
        }
        
        // Light
        ret = sensor_sample_fetch(mux1_light_sens);
        if (ret == -E2BIG) {
            printk("Light: OVERFLOW (>bright limit)\n");
        } else if (ret < 0) {
            printk("ERROR: Failed to fetch VEML6031 sample from channel 1 (error %d)\n", ret);
        } else {
            struct sensor_value light;
            ret = sensor_channel_get(mux1_light_sens, SENSOR_CHAN_LIGHT, &light);
            if (ret < 0) {
                printk("ERROR: Failed to get light channel from channel 1 (error %d)\n", ret);
            } else {
                double light_lux = sensor_value_to_double(&light);
                printk("Light: %.2f lux\n", light_lux);
            }
        }
        
        // Haptic - trigger a short vibration
        printk("Triggering haptic on channel 1...\n");
        ret = haptics_start_output(mux1_drv2605);
        if (ret < 0) {
            printk("ERROR: Failed to start haptic on channel 1 (error %d)\n", ret);
        } else {
            printk("Haptic: Started\n");
            k_sleep(K_MSEC(100));  // Vibrate for 100ms
            
            ret = haptics_stop_output(mux1_drv2605);
            if (ret < 0) {
                printk("ERROR: Failed to stop haptic on channel 1 (error %d)\n", ret);
            } else {
                printk("Haptic: Stopped\n");
            }
        }
    }

    return 0;
}
