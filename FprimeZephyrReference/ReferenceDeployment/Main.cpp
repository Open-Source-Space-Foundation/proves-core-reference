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
const struct device* ina219Sys = DEVICE_DT_GET(DT_NODELABEL(ina219_0));
const struct device* ina219Sol = DEVICE_DT_GET(DT_NODELABEL(ina219_1));
const struct device* serial = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));
const struct device* lora = DEVICE_DT_GET(DT_NODELABEL(lora0));
const struct device* lsm6dso = DEVICE_DT_GET(DT_NODELABEL(lsm6dso0));
const struct device* lis2mdl = DEVICE_DT_GET(DT_NODELABEL(lis2mdl0));
const struct device* rtc = DEVICE_DT_GET(DT_NODELABEL(rtc0));

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

    // Set up DRV2605 Devices
    const struct device* face0_drv2605 = DEVICE_DT_GET(DT_NODELABEL(face0_drv2605));
    const struct device* face1_drv2605 = DEVICE_DT_GET(DT_NODELABEL(face1_drv2605));
    const struct device* face2_drv2605 = DEVICE_DT_GET(DT_NODELABEL(face2_drv2605));
    const struct device* face3_drv2605 = DEVICE_DT_GET(DT_NODELABEL(face3_drv2605));
    const struct device* face4_drv2605 = DEVICE_DT_GET(DT_NODELABEL(face4_drv2605));

    if (gpio_is_ready_dt(&face0_enable) && gpio_pin_configure_dt(&face0_enable, GPIO_OUTPUT_INACTIVE) == 0) {
        gpio_pin_set_dt(&face0_enable, 1);
    } else {
        printk("Face 0 enable pin not ready\n");
    }

    if (gpio_is_ready_dt(&face1_enable) && gpio_pin_configure_dt(&face1_enable, GPIO_OUTPUT_INACTIVE) == 0) {
        gpio_pin_set_dt(&face1_enable, 1);
    } else {
        printk("Face 1 enable pin not ready\n");
    }

    if (gpio_is_ready_dt(&face2_enable) && gpio_pin_configure_dt(&face2_enable, GPIO_OUTPUT_INACTIVE) == 0) {
        gpio_pin_set_dt(&face2_enable, 1);
    } else {
        printk("Face 2 enable pin not ready\n");
    }

    if (gpio_is_ready_dt(&face3_enable) && gpio_pin_configure_dt(&face3_enable, GPIO_OUTPUT_INACTIVE) == 0) {
        gpio_pin_set_dt(&face3_enable, 1);
    } else {
        printk("Face 3 enable pin not ready\n");
    }

    k_sleep(K_MSEC(200));  // Wait for power to stabilize

    int ret = device_init(face0_drv2605);
    printk("DRV2605 Face 0 init returned: %d\n", ret);
    ret = device_init(face1_drv2605);
    printk("DRV2605 Face 1 init returned: %d\n", ret);
    ret = device_init(face2_drv2605);
    printk("DRV2605 Face 2 init returned: %d\n", ret);
    ret = device_init(face3_drv2605);
    printk("DRV2605 Face 3 init returned: %d\n", ret);
    ret = device_init(face4_drv2605);
    printk("DRV2605 Face 4 init returned: %d\n", ret);

    Os::init();
    // Object for communicating state to the topology
    ReferenceDeployment::TopologyState inputs;
    inputs.ina219SysDevice = ina219Sys;
    inputs.ina219SolDevice = ina219Sol;
    inputs.loraDevice = lora;
    inputs.uartDevice = serial;
    inputs.lsm6dsoDevice = lsm6dso;
    inputs.lis2mdlDevice = lis2mdl;
    inputs.rtcDevice = rtc;
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
