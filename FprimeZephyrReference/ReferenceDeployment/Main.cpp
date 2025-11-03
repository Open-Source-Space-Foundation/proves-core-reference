// ======================================================================
// \title  Main.cpp
// \brief main program for the F' application. Intended for CLI-based systems (Linux, macOS)
//
// ======================================================================
// Used to access topology functions

#include <FprimeZephyrReference/ReferenceDeployment/Top/ReferenceDeploymentTopology.hpp>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

const struct device* ina219Sys = DEVICE_DT_GET(DT_NODELABEL(ina219Sys));
const struct device* ina219Sol = DEVICE_DT_GET(DT_NODELABEL(ina219Sol));
const struct device* serial = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));
const struct device* lora = DEVICE_DT_GET(DT_NODELABEL(lora0));


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
    inputs.baudRate = 115200;
    


    // Setup, cycle, and teardown topology
    ReferenceDeployment::setupTopology(inputs);
    ReferenceDeployment::startRateGroups();  // Program loop
    ReferenceDeployment::teardownTopology(inputs);
    return 0;
}
