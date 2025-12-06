// ======================================================================
// \title  Main.cpp
// \brief main program for the F' application. Intended for CLI-based systems (Linux, macOS)
//
// ======================================================================
// Used to access topology functions

#include <FprimeZephyrReference/ReferenceDeployment/Top/ReferenceDeploymentTopology.hpp>

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

int main(int argc, char* argv[]) {
    // ** DO NOT REMOVE **//
    //
    // This sleep is necessary to allow the USB CDC ACM interface to initialize before
    // the application starts writing to it.
    k_sleep(K_MSEC(3000));
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

    inputs.baudRate = 115200;

    // Setup, cycle, and teardown topology
    ReferenceDeployment::setupTopology(inputs);
    ReferenceDeployment::startRateGroups();  // Program loop
    ReferenceDeployment::teardownTopology(inputs);
    return 0;
}
