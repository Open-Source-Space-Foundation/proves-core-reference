// ======================================================================
// \title  Main.cpp
// \brief main program for the F' application. Intended for CLI-based systems (Linux, macOS)
//
// ======================================================================
// Used to access topology functions

#include <FprimeZephyrReference/ReferenceDeployment/Top/ReferenceDeploymentTopology.hpp>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

const struct device* serial = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));

int main(int argc, char* argv[]) {
    // ** DO NOT REMOVE **//
    //
    // This sleep is necessary to allow the USB CDC ACM interface to initialize before
    // the application starts writing to it.
    k_sleep(K_MSEC(3000));

    printk("finished sleeps");
    Os::init();
    printk("initined\n");
    // Object for communicating state to the topology
    ReferenceDeployment::TopologyState inputs;
    printk("created state input object");
    inputs.uartDevice = serial;
    inputs.baudRate = 115200;

    // Setup, cycle, and teardown topology
    printk("starting to setup the topology\n");
    ReferenceDeployment::setupTopology(inputs);
    printk("finished setting up the topology\n");
    ReferenceDeployment::startRateGroups();  // Program loop
    printk("finished rate groups\n");
    ReferenceDeployment::teardownTopology(inputs);
    return 0;
}
