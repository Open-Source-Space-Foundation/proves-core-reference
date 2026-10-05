// ======================================================================
// \title  Main.cpp
// \brief main program for the F' application. Intended for CLI-based systems (Linux, macOS)
//
// ======================================================================
// Used to access topology functions
#include <PROVESFlightControllerReference/NiclaDeployment/Top/NiclaDeploymentTopology.hpp>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

const struct device* serial = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

int main(int argc, char* argv[]) {
    // ** DO NOT REMOVE **//
    //
    // This sleep is necessary to allow the USB CDC ACM interface to initialize before
    // the application starts writing to it.
    k_sleep(K_MSEC(3000));

    Os::init();
    // Object for communicating state to the topology
    NiclaDeployment::TopologyState inputs;
    inputs.uartDevice = serial;
    inputs.baudRate = 115200;

    // Setup, cycle, and teardown topology
    NiclaDeployment::setupTopology(inputs);
    printk("Starting topology.....\n");
    NiclaDeployment::startRateGroups();  // Program loop
    NiclaDeployment::teardownTopology(inputs);
    return 0;
}
