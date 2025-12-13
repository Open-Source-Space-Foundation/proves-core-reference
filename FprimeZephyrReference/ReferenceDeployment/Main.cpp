// ======================================================================
// \title  Main.cpp
// \brief main program for the F' application. Intended for CLI-based systems (Linux, macOS)
//
// ======================================================================
// Used to access topology functions

#include <FprimeZephyrReference/ReferenceDeployment/Top/ReferenceDeploymentTopology.hpp>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/haptics.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

// RadioLib SX1280 test includes (Phase 1 validation)
#ifdef CONFIG_RADIOLIB_SX1280_TEST
#include "RadioLibZephyrHal.hpp"
#include "modules/SX128x/SX1280.h"
#include "modules/SX128x/SX128x.h"

// Pin number definitions for RadioLib Module constructor
// These match the pin mapping in RadioLibZephyrHal
#define RADIOLIB_PIN_CS 0
#define RADIOLIB_PIN_RST 1
#define RADIOLIB_PIN_BUSY 2
#define RADIOLIB_PIN_DIO1 3
#endif

// Devices
const struct device* ina219Sys = DEVICE_DT_GET(DT_NODELABEL(ina219_0));
const struct device* ina219Sol = DEVICE_DT_GET(DT_NODELABEL(ina219_1));
const struct device* serial = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));
const struct device* lora = DEVICE_DT_GET(DT_NODELABEL(lora0));
const struct device* peripheral_uart = DEVICE_DT_GET(DT_NODELABEL(uart0));
const struct device* peripheral_uart1 = DEVICE_DT_GET(DT_NODELABEL(uart1));
const struct device* lsm6dso = DEVICE_DT_GET(DT_NODELABEL(lsm6dso0));
const struct device* lis2mdl = DEVICE_DT_GET(DT_NODELABEL(lis2mdl0));
const struct device* rtc = DEVICE_DT_GET(DT_NODELABEL(rtc0));
const struct device* tca9548a = DEVICE_DT_GET(DT_NODELABEL(tca9548a));
const struct device* mux_channel_0 = DEVICE_DT_GET(DT_NODELABEL(mux_channel_0));
const struct device* mux_channel_1 = DEVICE_DT_GET(DT_NODELABEL(mux_channel_1));
const struct device* mux_channel_2 = DEVICE_DT_GET(DT_NODELABEL(mux_channel_2));
const struct device* mux_channel_3 = DEVICE_DT_GET(DT_NODELABEL(mux_channel_3));
const struct device* mux_channel_4 = DEVICE_DT_GET(DT_NODELABEL(mux_channel_4));
const struct device* mux_channel_5 = DEVICE_DT_GET(DT_NODELABEL(mux_channel_5));
const struct device* mux_channel_6 = DEVICE_DT_GET(DT_NODELABEL(mux_channel_6));
const struct device* mux_channel_7 = DEVICE_DT_GET(DT_NODELABEL(mux_channel_7));
const struct device* face0_temp_sens = DEVICE_DT_GET(DT_NODELABEL(face0_temp_sens));
const struct device* face1_temp_sens = DEVICE_DT_GET(DT_NODELABEL(face1_temp_sens));
const struct device* face2_temp_sens = DEVICE_DT_GET(DT_NODELABEL(face2_temp_sens));
const struct device* face3_temp_sens = DEVICE_DT_GET(DT_NODELABEL(face3_temp_sens));
const struct device* face5_temp_sens = DEVICE_DT_GET(DT_NODELABEL(face5_temp_sens));
const struct device* batt_cell1_temp_sens = DEVICE_DT_GET(DT_NODELABEL(batt_cell1_temp_sens));
const struct device* batt_cell2_temp_sens = DEVICE_DT_GET(DT_NODELABEL(batt_cell2_temp_sens));
const struct device* batt_cell3_temp_sens = DEVICE_DT_GET(DT_NODELABEL(batt_cell3_temp_sens));
const struct device* batt_cell4_temp_sens = DEVICE_DT_GET(DT_NODELABEL(batt_cell4_temp_sens));
const struct device* face0_light_sens = DEVICE_DT_GET(DT_NODELABEL(face0_light_sens));
const struct device* face1_light_sens = DEVICE_DT_GET(DT_NODELABEL(face1_light_sens));
const struct device* face2_light_sens = DEVICE_DT_GET(DT_NODELABEL(face2_light_sens));
const struct device* face3_light_sens = DEVICE_DT_GET(DT_NODELABEL(face3_light_sens));
const struct device* face5_light_sens = DEVICE_DT_GET(DT_NODELABEL(face5_light_sens));
const struct device* face6_light_sens = DEVICE_DT_GET(DT_NODELABEL(face6_light_sens));
const struct device* face7_light_sens = DEVICE_DT_GET(DT_NODELABEL(face7_light_sens));
const struct device* face0_drv2605 = DEVICE_DT_GET(DT_NODELABEL(face0_drv2605));
const struct device* face1_drv2605 = DEVICE_DT_GET(DT_NODELABEL(face1_drv2605));
const struct device* face2_drv2605 = DEVICE_DT_GET(DT_NODELABEL(face2_drv2605));
const struct device* face3_drv2605 = DEVICE_DT_GET(DT_NODELABEL(face3_drv2605));
const struct device* face5_drv2605 = DEVICE_DT_GET(DT_NODELABEL(face5_drv2605));

#ifdef CONFIG_RADIOLIB_SX1280_TEST
// SX1280 device and GPIO definitions for RadioLib test
const struct device* sx1280_spi = DEVICE_DT_GET(DT_NODELABEL(spi0));

// CS pin comes from SPI device tree (second CS on spi0, GPIO 7)
static const struct gpio_dt_spec sx1280_cs_gpio = GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(spi0), cs_gpios, 1);

// RESET pin: GPIO 17 on gpio0
// Note: Not using GPIO_ACTIVE_LOW flag - we'll handle reset logic directly
// RESET should be LOW to reset, HIGH for normal operation
static const struct gpio_dt_spec sx1280_reset_gpio = {
    .port = DEVICE_DT_GET(DT_NODELABEL(gpio0)),
    .pin = 17,
    .dt_flags = 0  // No active-low flag - use normal GPIO levels
};

// BUSY pin: GPIO 14 on MCP23017 GPIO expander
static const struct gpio_dt_spec sx1280_busy_gpio = {
    .port = DEVICE_DT_GET(DT_NODELABEL(mcp23017)),
    .pin = 14,
    .dt_flags = 0  // Input pin, no interrupt
};

// DIO1 pin: GPIO 13 on MCP23017 GPIO expander
static const struct gpio_dt_spec sx1280_dio1_gpio = {
    .port = DEVICE_DT_GET(DT_NODELABEL(mcp23017)),
    .pin = 13,
    .dt_flags = 0  // Input pin, no interrupt
};

/*!
 * \brief Test function to validate RadioLib SX1280 integration at Zephyr level
 *
 * This function:
 * 1. Initializes the Zephyr HAL adapter
 * 2. Initializes RadioLib SX1280 module
 * 3. Tests basic radio operations (register reads, configuration)
 *
 * Returns 0 on success, negative error code on failure
 */
static int test_sx1280_radiolib() {
    printk("\n=== RadioLib SX1280 Test Start ===\n");

    // Verify MCP23017 is ready (needed for BUSY and DIO1 pins)
    const struct device* mcp23017_dev = DEVICE_DT_GET(DT_NODELABEL(mcp23017));
    if (!device_is_ready(mcp23017_dev)) {
        printk("ERROR: MCP23017 GPIO expander not ready!\n");
        printk("Waiting for MCP23017 to initialize...\n");
        // Wait a bit for MCP23017 to initialize
        k_sleep(K_MSEC(100));
        if (!device_is_ready(mcp23017_dev)) {
            printk("ERROR: MCP23017 still not ready after delay!\n");
            return -ENODEV;
        }
    }
    printk("MCP23017 GPIO expander ready\n");

    // Initialize HAL
    ZephyrHal hal(sx1280_spi, &sx1280_cs_gpio, &sx1280_reset_gpio, &sx1280_busy_gpio, &sx1280_dio1_gpio);

    hal.init();  // Initialize HAL (returns void, calls initHal() internally)
    printk("RadioLib HAL initialized\n");

    // Verify GPIO pin states
    if (device_is_ready(sx1280_busy_gpio.port)) {
        int busy_val = gpio_pin_get_dt(&sx1280_busy_gpio);
        printk("BUSY pin initial state: %d\n", busy_val);
    }

    // Verify CS pin is initially high (inactive)
    if (device_is_ready(sx1280_cs_gpio.port)) {
        int cs_val = gpio_pin_get_dt(&sx1280_cs_gpio);
        printk("CS pin initial state: %d (should be 1 = inactive/high for active-low CS)\n", cs_val);
    }

    // Verify SPI device is ready
    if (device_is_ready(sx1280_spi)) {
        printk("SPI0 device is ready\n");
    } else {
        printk("ERROR: SPI0 device not ready!\n");
        return -ENODEV;
    }

    // Create RadioLib Module (with pin numbers matching our HAL mapping)
    // BUSY pin is critical for SX1280 - RadioLib will wait for it to go LOW before SPI transactions
    Module mod(&hal, RADIOLIB_PIN_CS, RADIOLIB_PIN_DIO1, RADIOLIB_PIN_RST, RADIOLIB_PIN_BUSY);

    // Create SX1280 instance
    SX1280 radio(&mod);

    // Initialize SX1280 (LoRa mode)
    // Parameters: freq=2400MHz, bw=812.5kHz, sf=9, cr=7, syncWord, pwr=10dBm, preambleLength=12
    printk("Initializing SX1280...\n");
    int16_t state = radio.begin(2400.0, 812.5, 9, 7, RADIOLIB_SX128X_SYNC_WORD_PRIVATE, 10, 12);

    if (state != RADIOLIB_ERR_NONE) {
        printk("ERROR: SX1280 initialization failed: %d\n", state);
        return -1;
    }
    printk("SUCCESS: SX1280 initialized\n");

    // Test basic configuration
    printk("Testing radio configuration...\n");

    // Set frequency
    state = radio.setFrequency(2400.0);
    if (state != RADIOLIB_ERR_NONE) {
        printk("WARNING: Failed to set frequency: %d\n", state);
    } else {
        printk("SUCCESS: Frequency set to 2400 MHz\n");
    }

    // Set TX power
    state = radio.setOutputPower(13);  // 13 dBm
    if (state != RADIOLIB_ERR_NONE) {
        printk("WARNING: Failed to set TX power: %d\n", state);
    } else {
        printk("SUCCESS: TX power set to 13 dBm\n");
    }

    // Read chip version (if available via RadioLib API)
    printk("RadioLib SX1280 test completed successfully!\n");
    printk("=== RadioLib SX1280 Test End ===\n\n");

    return 0;
}
#endif

int main(int argc, char* argv[]) {
    //
    // This sleep is necessary to allow the USB CDC ACM interface to initialize before
    // the application starts writing to it.
    k_sleep(K_MSEC(3000));

#ifdef CONFIG_RADIOLIB_SX1280_TEST
    // Phase 1: Test RadioLib integration at Zephyr level before F Prime setup
    printk("Running RadioLib SX1280 Zephyr-level test...\n");
    int test_result = test_sx1280_radiolib();
    if (test_result != 0) {
        printk("RadioLib test failed, continuing with normal startup...\n");
    }
    // Continue with normal startup regardless of test result
#endif

    Os::init();

    // Object for communicating state to the topology
    ReferenceDeployment::TopologyState inputs;

    // Flight Control Board device bindings
    inputs.ina219SysDevice = ina219Sys;
    inputs.ina219SolDevice = ina219Sol;
    inputs.loraDevice = lora;
    inputs.uartDevice = serial;
    inputs.lsm6dsoDevice = lsm6dso;
    inputs.lis2mdlDevice = lis2mdl;
    inputs.rtcDevice = rtc;
    inputs.tca9548aDevice = tca9548a;
    inputs.muxChannel0Device = mux_channel_0;
    inputs.muxChannel1Device = mux_channel_1;
    inputs.muxChannel2Device = mux_channel_2;
    inputs.muxChannel3Device = mux_channel_3;
    inputs.muxChannel4Device = mux_channel_4;
    inputs.muxChannel5Device = mux_channel_5;
    inputs.muxChannel6Device = mux_channel_6;
    inputs.muxChannel7Device = mux_channel_7;

    // Face Board device bindings
    // TMP112 temperature sensor devices
    inputs.face0TempDevice = face0_temp_sens;
    inputs.face1TempDevice = face1_temp_sens;
    inputs.face2TempDevice = face2_temp_sens;
    inputs.face3TempDevice = face3_temp_sens;
    inputs.face5TempDevice = face5_temp_sens;
    inputs.battCell1TempDevice = batt_cell1_temp_sens;
    inputs.battCell2TempDevice = batt_cell2_temp_sens;
    inputs.battCell3TempDevice = batt_cell3_temp_sens;
    inputs.battCell4TempDevice = batt_cell4_temp_sens;
    // Light sensor devices
    inputs.face0LightDevice = face0_light_sens;
    inputs.face1LightDevice = face1_light_sens;
    inputs.face2LightDevice = face2_light_sens;
    inputs.face3LightDevice = face3_light_sens;
    inputs.face5LightDevice = face5_light_sens;
    inputs.face6LightDevice = face6_light_sens;
    inputs.face7LightDevice = face7_light_sens;
    // Magnetorquer devices
    inputs.face0drv2605Device = face0_drv2605;
    inputs.face1drv2605Device = face1_drv2605;
    inputs.face2drv2605Device = face2_drv2605;
    inputs.face3drv2605Device = face3_drv2605;
    inputs.face5drv2605Device = face5_drv2605;
    inputs.baudRate = 115200;

    // For the uart peripheral config
    inputs.peripheralBaudRate = 115200;  // Minimum is 19200
    inputs.peripheralUart = peripheral_uart;
    inputs.peripheralBaudRate2 = 115200;  // Minimum is 19200
    inputs.peripheralUart2 = peripheral_uart1;

    // Setup, cycle, and teardown topology
    ReferenceDeployment::setupTopology(inputs);
    ReferenceDeployment::startRateGroups();  // Program loop
    ReferenceDeployment::teardownTopology(inputs);
    return 0;
}
