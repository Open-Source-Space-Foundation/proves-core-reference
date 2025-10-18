// ======================================================================
// \title  Main.cpp
// \brief main program for the F' application. Intended for CLI-based systems (Linux, macOS)
//
// ======================================================================
// Used to access topology functions

#include <FprimeZephyrReference/ReferenceDeployment/Top/ReferenceDeploymentTopology.hpp>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <Os/File.hpp>

const struct device* serial = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));
const struct device* lora = DEVICE_DT_GET(DT_NODELABEL(lora0));

int main(int argc, char* argv[]) {
    // ** DO NOT REMOVE **//
    //
    // This sleep is necessary to allow the USB CDC ACM interface to initialize before
    // the application starts writing to it.
    k_sleep(K_MSEC(3000));
    Os::File file;
    U8 message[] = "Hello Ines\n";
    U8 message1[sizeof(message) + 1] = {0};
    Os::File::Status status;
    FwSizeType size;
    // = file.open("/tmp1", Os::File::Mode::OPEN_CREATE);
    // Os::File::Status status = file.open("/tmp1", Os::File::Mode::OPEN_CREATE);
    // FwSizeType size = static_cast<FwSignedSizeType>(sizeof(message));
    // printk("Status: %d - open\n", static_cast<int>(status));
    // status = file.write(message, size);
    // printk("Status: %d - write %" PRI_FwSizeType "\n", static_cast<int>(status), size);
    // file.close();
    status = file.open("/tmp1", Os::File::Mode::OPEN_READ);
    printk("Status: %d - open (R)\n", static_cast<int>(status));
    size = static_cast<FwSignedSizeType>(sizeof(message));
    status = file.read(message1, size);
    printk("Status: %d - read %" PRI_FwSizeType "\n", static_cast<int>(status), size);
    message1[sizeof(message)] = 0;
    printk("Message: %s\n", message1);
    Os::init();
    // Object for communicating state to the topology
    ReferenceDeployment::TopologyState inputs;
    inputs.loraDevice = lora;
    inputs.uartDevice = serial;
    inputs.baudRate = 115200;

    // Setup, cycle, and teardown topology
    ReferenceDeployment::setupTopology(inputs);
    ReferenceDeployment::startRateGroups();  // Program loop
    ReferenceDeployment::teardownTopology(inputs);
    return 0;
}
