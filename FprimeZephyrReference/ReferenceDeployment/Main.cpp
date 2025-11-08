// ======================================================================
// \title  Main.cpp
// \brief main program for the F' application. Intended for CLI-based systems (Linux, macOS)
//
// ======================================================================
// Used to access topology functions

#include <FprimeZephyrReference/ReferenceDeployment/Top/ReferenceDeploymentTopology.hpp>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

const struct device* ina219Sys = device_get_binding("INA219 sys");
const struct device* ina219Sol = device_get_binding("INA219 sol");
const struct device* serial = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));
const struct device* lora = DEVICE_DT_GET(DT_NODELABEL(lora0));
const struct device* lsm6dso = DEVICE_DT_GET(DT_NODELABEL(lsm6dso0));
const struct device* lis2mdl = DEVICE_DT_GET(DT_NODELABEL(lis2mdl0));

// TMP112 temperature sensors (11 sensors behind I2C multiplexer)
const struct device* face0_temp = DEVICE_DT_GET(DT_NODELABEL(face0_temp_sens));
const struct device* face1_temp = DEVICE_DT_GET(DT_NODELABEL(face1_temp_sens));
const struct device* face2_temp = DEVICE_DT_GET(DT_NODELABEL(face2_temp_sens));
const struct device* face3_temp = DEVICE_DT_GET(DT_NODELABEL(face3_temp_sens));
const struct device* face4_temp = DEVICE_DT_GET(DT_NODELABEL(face4_temp_sens));
const struct device* face5_temp = DEVICE_DT_GET(DT_NODELABEL(face5_temp_sens));
const struct device* top_temp = DEVICE_DT_GET(DT_NODELABEL(top_temp_sens));
const struct device* batt_cell1_temp = DEVICE_DT_GET(DT_NODELABEL(batt_cell1_temp_sens));
const struct device* batt_cell2_temp = DEVICE_DT_GET(DT_NODELABEL(batt_cell2_temp_sens));
const struct device* batt_cell3_temp = DEVICE_DT_GET(DT_NODELABEL(batt_cell3_temp_sens));
const struct device* batt_cell4_temp = DEVICE_DT_GET(DT_NODELABEL(batt_cell4_temp_sens));

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

    // TMP112 temperature sensor devices
    inputs.face0TempDevice = face0_temp;
    inputs.face1TempDevice = face1_temp;
    inputs.face2TempDevice = face2_temp;
    inputs.face3TempDevice = face3_temp;
    inputs.face4TempDevice = face4_temp;
    inputs.face5TempDevice = face5_temp;
    inputs.topTempDevice = top_temp;
    inputs.battCell1TempDevice = batt_cell1_temp;
    inputs.battCell2TempDevice = batt_cell2_temp;
    inputs.battCell3TempDevice = batt_cell3_temp;
    inputs.battCell4TempDevice = batt_cell4_temp;

    inputs.baudRate = 115200;

    // Setup, cycle, and teardown topology
    ReferenceDeployment::setupTopology(inputs);
    ReferenceDeployment::startRateGroups();  // Program loop
    ReferenceDeployment::teardownTopology(inputs);
    return 0;
}
