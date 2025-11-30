// ======================================================================
// \title  Main.cpp
// \brief main program for the F' application. Intended for CLI-based systems (Linux, macOS)
//
// ======================================================================
// Used to access topology functions

#include <FprimeZephyrReference/ReferenceDeployment/Top/ReferenceDeploymentTopology.hpp>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

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

    inputs.baudRate = 115200;

    // Setup, cycle, and teardown topology
    ReferenceDeployment::setupTopology(inputs);
    ReferenceDeployment::startRateGroups();  // Program loop
    ReferenceDeployment::teardownTopology(inputs);
    return 0;
}
