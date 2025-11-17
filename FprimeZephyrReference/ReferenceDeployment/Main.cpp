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

#include <FprimeZephyrReference/ReferenceDeployment/Top/ReferenceDeploymentTopology.hpp>

// Devices
const struct device* ina219Sys = device_get_binding("INA219 sys");
const struct device* ina219Sol = device_get_binding("INA219 sol");
const struct device* serial = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));
const struct device* lora = DEVICE_DT_GET(DT_NODELABEL(lora0));
const struct device* lsm6dso = DEVICE_DT_GET(DT_NODELABEL(lsm6dso0));
const struct device* lis2mdl = DEVICE_DT_GET(DT_NODELABEL(lis2mdl0));

// TCA Setup
const struct device* tca9548a = DEVICE_DT_GET(DT_NODELABEL(tca9548a));
const struct device* mux_channel_0 = DEVICE_DT_GET(DT_NODELABEL(mux_channel_0));
const struct device* mux_channel_1 = DEVICE_DT_GET(DT_NODELABEL(mux_channel_1));
const struct device* mux_channel_2 = DEVICE_DT_GET(DT_NODELABEL(mux_channel_2));
const struct device* mux_channel_3 = DEVICE_DT_GET(DT_NODELABEL(mux_channel_3));
const struct device* mux_channel_4 = DEVICE_DT_GET(DT_NODELABEL(mux_channel_4));

// Face enable pins from MCP23017 GPIO expander
static const struct gpio_dt_spec face0_enable = GPIO_DT_SPEC_GET(DT_NODELABEL(face0_enable), gpios);
static const struct gpio_dt_spec face1_enable = GPIO_DT_SPEC_GET(DT_NODELABEL(face1_enable), gpios);
static const struct gpio_dt_spec face2_enable = GPIO_DT_SPEC_GET(DT_NODELABEL(face2_enable), gpios);
static const struct gpio_dt_spec face3_enable = GPIO_DT_SPEC_GET(DT_NODELABEL(face3_enable), gpios);

if (gpio_is_ready_dt(&face0_enable) && gpio_pin_configure_dt(&face0_enable, GPIO_OUTPUT_INACTIVE) == 0) {
    gpio_pin_set_dt(&face0_enable, 1);
}

if (gpio_is_ready_dt(&face1_enable) && gpio_pin_configure_dt(&face1_enable, GPIO_OUTPUT_INACTIVE) == 0) {
    gpio_pin_set_dt(&face1_enable, 1);
}

if (gpio_is_ready_dt(&face2_enable) && gpio_pin_configure_dt(&face2_enable, GPIO_OUTPUT_INACTIVE) == 0) {
    gpio_pin_set_dt(&face2_enable, 1);
}

if (gpio_is_ready_dt(&face3_enable) && gpio_pin_configure_dt(&face3_enable, GPIO_OUTPUT_INACTIVE) == 0) {
    gpio_pin_set_dt(&face3_enable, 1);
}

k_sleep(K_MSEC(200));  // Wait for power to stabilize

// Set up DRV2605 Devices
const struct device* face0_drv2605 = DEVICE_DT_GET(DT_NODELABEL(mux0_drv2605));
const struct device* face1_drv2605 = DEVICE_DT_GET(DT_NODELABEL(mux1_drv2605));
const struct device* face2_drv2605 = DEVICE_DT_GET(DT_NODELABEL(mux2_drv2605));
const struct device* face3_drv2605 = DEVICE_DT_GET(DT_NODELABEL(mux3_drv2605));
const struct device* face4_drv2605 = DEVICE_DT_GET(DT_NODELABEL(mux4_drv2605));

int ret = device_init(face0_drv2605);
ret = device_init(face1_drv2605);
ret = device_init(face2_drv2605);
ret = device_init(face3_drv2605);
ret = device_init(face4_drv2605);

int main(int argc, char* argv[]) {
    // ** DO NOT REMOVE **//
    //
    // This sleep is necessary to allow the USB CDC ACM interface to initialize before
    // the application starts writing to it.
    k_sleep(K_MSEC(3000));
    Os::init();
    // Object for communicating state to the topology
    ReferenceDeployment::TopologyState inputs;
    inputs.ina219SysDevice = ina219Sys;
    inputs.ina219SolDevice = ina219Sol;
    inputs.loraDevice = lora;
    inputs.uartDevice = serial;
    inputs.lsm6dsoDevice = lsm6dso;
    inputs.lis2mdlDevice = lis2mdl;
    inputs.drv2605Devices[0] = face0_drv2605;
    inputs.drv2605Devices[1] = face1_drv2605;
    inputs.drv2605Devices[2] = face2_drv2605;
    inputs.drv2605Devices[3] = face3_drv2605;
    inputs.drv2605Devices[4] = face4_drv2605;
    inputs.baudRate = 115200;

    // Setup, cycle, and teardown topology
    ReferenceDeployment::setupTopology(inputs);
    ReferenceDeployment::startRateGroups();  // Program loop
    ReferenceDeployment::teardownTopology(inputs);
    return 0;
}
