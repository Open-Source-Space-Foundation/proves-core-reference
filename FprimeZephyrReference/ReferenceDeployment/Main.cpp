// ======================================================================
// \title  Main.cpp
// \brief main program for the F' application. Intended for CLI-based systems (Linux, macOS)
//
// ======================================================================
// Used to access topology functions

#include <FprimeZephyrReference/ReferenceDeployment/Top/ReferenceDeploymentTopology.hpp>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

// Device bindings
const struct device* ina219Sys = DEVICE_DT_GET(DT_NODELABEL(ina219_0));
const struct device* ina219Sol = DEVICE_DT_GET(DT_NODELABEL(ina219_1));
const struct device* serial = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));
const struct device* lora = DEVICE_DT_GET(DT_NODELABEL(lora0));
const struct device* lsm6dso = DEVICE_DT_GET(DT_NODELABEL(lsm6dso0));
const struct device* lis2mdl = DEVICE_DT_GET(DT_NODELABEL(lis2mdl0));
const struct device* rtc = DEVICE_DT_GET(DT_NODELABEL(rtc0));
const struct device* mcp23017 = DEVICE_DT_GET(DT_NODELABEL(mcp23017));
const struct device* face0_temp_sens = DEVICE_DT_GET(DT_NODELABEL(face0_temp_sens));
const struct device* face1_temp_sens = DEVICE_DT_GET(DT_NODELABEL(face1_temp_sens));
const struct device* face2_temp_sens = DEVICE_DT_GET(DT_NODELABEL(face2_temp_sens));
const struct device* face3_temp_sens = DEVICE_DT_GET(DT_NODELABEL(face3_temp_sens));
const struct device* face4_temp_sens = DEVICE_DT_GET(DT_NODELABEL(face4_temp_sens));
const struct device* face5_temp_sens = DEVICE_DT_GET(DT_NODELABEL(face5_temp_sens));
const struct device* top_temp_sens = DEVICE_DT_GET(DT_NODELABEL(top_temp_sens));
const struct device* batt_cell1_temp_sens = DEVICE_DT_GET(DT_NODELABEL(batt_cell1_temp_sens));
const struct device* batt_cell2_temp_sens = DEVICE_DT_GET(DT_NODELABEL(batt_cell2_temp_sens));
const struct device* batt_cell3_temp_sens = DEVICE_DT_GET(DT_NODELABEL(batt_cell3_temp_sens));
const struct device* batt_cell4_temp_sens = DEVICE_DT_GET(DT_NODELABEL(batt_cell4_temp_sens));

int main(int argc, char* argv[]) {
    // ** DO NOT REMOVE **//
    //
    // This sleep is necessary to allow the USB CDC ACM interface to initialize before
    // the application starts writing to it.
    k_sleep(K_MSEC(3000));
    Os::init();

    // NOTE: Load switches are now powered on during early boot (SYS_INIT)
    // before sensor initialization. See proves_board_power_init() above.

    // Object for communicating state to the topology
    ReferenceDeployment::TopologyState inputs;
    inputs.ina219SysDevice = ina219Sys;
    inputs.ina219SolDevice = ina219Sol;
    inputs.loraDevice = lora;
    inputs.uartDevice = serial;
    inputs.lsm6dsoDevice = lsm6dso;
    inputs.lis2mdlDevice = lis2mdl;
    inputs.rtcDevice = rtc;
    inputs.mcp23017 = mcp23017;
    inputs.baudRate = 115200;

    // TMP112 temperature sensor devices
    inputs.face0TempDevice = face0_temp_sens;
    inputs.face1TempDevice = face1_temp_sens;
    inputs.face2TempDevice = face2_temp_sens;
    inputs.face3TempDevice = face3_temp_sens;
    inputs.face4TempDevice = face4_temp_sens;
    inputs.face5TempDevice = face5_temp_sens;
    inputs.topTempDevice = top_temp_sens;
    inputs.battCell1TempDevice = batt_cell1_temp_sens;
    inputs.battCell2TempDevice = batt_cell2_temp_sens;
    inputs.battCell3TempDevice = batt_cell3_temp_sens;
    inputs.battCell4TempDevice = batt_cell4_temp_sens;

    // Diagnostic: Check which TMP112 sensors are ready
    // printk("\n=== TMP112 Sensor Ready Status ===\n");
    // const struct device* sensors[] = {
    //     inputs.face0TempDevice, inputs.face1TempDevice,
    //     inputs.face2TempDevice, inputs.face3TempDevice,
    //     inputs.face4TempDevice, inputs.face5TempDevice,
    //     inputs.topTempDevice,
    //     inputs.battCell1TempDevice, inputs.battCell2TempDevice,
    //     inputs.battCell3TempDevice, inputs.battCell4TempDevice
    // };
    // const char* names[] = {"face0", "face1", "face2", "face3",
    //                        "face4", "face5", "top",
    //                        "batt1", "batt2", "batt3", "batt4"};
    // for(int i=0; i<11; i++) {
    //     int ready = device_is_ready(sensors[i]);
    //     printk("  %s: %s (ptr: %p)\n", names[i], ready ? "READY" : "NOT READY", sensors[i]);
    // }
    // printk("==================================\n\n");

    // Setup, cycle, and teardown topology
    ReferenceDeployment::setupTopology(inputs);
    ReferenceDeployment::startRateGroups();  // Program loop
    ReferenceDeployment::teardownTopology(inputs);
    return 0;
}
