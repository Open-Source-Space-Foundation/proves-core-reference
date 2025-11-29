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

// Devices
const struct device* ina219Sys = DEVICE_DT_GET(DT_NODELABEL(ina219_0));
const struct device* ina219Sol = DEVICE_DT_GET(DT_NODELABEL(ina219_1));
const struct device* serial = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));
const struct device* lora = DEVICE_DT_GET(DT_NODELABEL(lora0));
const struct device* lsm6dso = DEVICE_DT_GET(DT_NODELABEL(lsm6dso0));
const struct device* lis2mdl = DEVICE_DT_GET(DT_NODELABEL(lis2mdl0));
const struct device* rtc = DEVICE_DT_GET(DT_NODELABEL(rtc0));
const struct device* face0_drv2605 = DEVICE_DT_GET(DT_NODELABEL(face0_drv2605));
const struct device* face1_drv2605 = DEVICE_DT_GET(DT_NODELABEL(face1_drv2605));
const struct device* face2_drv2605 = DEVICE_DT_GET(DT_NODELABEL(face2_drv2605));
const struct device* face3_drv2605 = DEVICE_DT_GET(DT_NODELABEL(face3_drv2605));
const struct device* face5_drv2605 = DEVICE_DT_GET(DT_NODELABEL(face5_drv2605));

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
    inputs.rtcDevice = rtc;
    inputs.drv2605Devices["X+"] = face0_drv2605;
    inputs.drv2605Devices["X-"] = face1_drv2605;
    inputs.drv2605Devices["Y+"] = face2_drv2605;
    inputs.drv2605Devices["Y-"] = face3_drv2605;
    inputs.drv2605Devices["Z+"] = face5_drv2605;
    inputs.baudRate = 115200;

    // Setup, cycle, and teardown topology
    ReferenceDeployment::setupTopology(inputs);
    ReferenceDeployment::startRateGroups();  // Program loop
    ReferenceDeployment::teardownTopology(inputs);
    return 0;
}
