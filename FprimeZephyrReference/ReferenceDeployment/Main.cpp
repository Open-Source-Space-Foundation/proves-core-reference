// ======================================================================
// \title  Main.cpp
// \brief main program for the F' application. Intended for CLI-based systems (Linux, macOS)
//
// ======================================================================
// Used to access topology functions

#include <lib/sparkfun-pico/sparkfun_pico/sfe_psram_zephyr.h>

#include <FprimeZephyrReference/ReferenceDeployment/Top/ReferenceDeploymentTopology.hpp>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/haptics.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(main);

// Devices
const struct device* ina219Sys = DEVICE_DT_GET(DT_NODELABEL(ina219_0));
const struct device* ina219Sol = DEVICE_DT_GET(DT_NODELABEL(ina219_1));
const struct device* serial = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));
const struct device* lora = DEVICE_DT_GET(DT_NODELABEL(lora0));
// const struct device* spi0 = DEVICE_DT_GET(DT_NODELABEL(spi0));
const struct device* peripheral_uart = DEVICE_DT_GET(DT_NODELABEL(uart0));
const struct device* peripheral_uart1 = DEVICE_DT_GET(DT_NODELABEL(uart1));
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
const struct device* face0_drv2605 = DEVICE_DT_GET(DT_NODELABEL(face0_drv2605));
const struct device* face1_drv2605 = DEVICE_DT_GET(DT_NODELABEL(face1_drv2605));
const struct device* face2_drv2605 = DEVICE_DT_GET(DT_NODELABEL(face2_drv2605));
const struct device* face3_drv2605 = DEVICE_DT_GET(DT_NODELABEL(face3_drv2605));
const struct device* face5_drv2605 = DEVICE_DT_GET(DT_NODELABEL(face5_drv2605));
const int storage_partition_id = FIXED_PARTITION_ID(storage_partition);

/* APS1604M instruction set */
#define APS1604M_CMD_READ 0x03U       // Read Memory Code
#define APS1604M_CMD_FAST_READ 0x0BU  // Fast Read Memory Code
#define APS1604M_CMD_READ_QUAD 0xEBU  // Quad Read Memory Code (

#define APS1604M_CMD_WRITE 0x02U       // Write Memory Code
#define APS1604M_CMD_WRITE_QUAD 0x38U  // Quad Write Memory Code

#define APS1604M_CMD_WRAPPED_READ 0x8BU   // Wrapped Read Memory Code
#define APS1604M_CMD_WRAPPED_WRITE 0x82U  // Wrapped Write Memory Code

#define APS1604M_CMD_REGISTER_READ 0xB5U   // Register Read Memory Code
#define APS1604M_CMD_REGISTER_WRITE 0xB1U  // Register Write Memory Code

#define APS1604M_CMD_ENTER_QUAD_MODE 0x35U  // Enter Quad Mode
#define APS1604M_CMD_EXIT_QUAD_MODE 0xF5U   // Exit Quad Mode

#define APS1604M_CMD_RESET_ENABLE 0x66U  // Reset Enable
#define APS1604M_CMD_RESET 0x99U         // Reset

#define APS1604M_CMD_BURST_LENGTH_TOGGLE 0xC0U  // Burst Length Toggle
#define APS1604M_CMD_READ_ID 0x9FU

/* PSRAM CS pin: from devicetree psram0  */
#define PSRAM_CS_PIN DT_GPIO_PIN(DT_NODELABEL(psram0), cs_gpios)

int main(int argc, char* argv[]) {
    printk("main() started\n");
    LOG_INF("main() started");
    //
    // Brief delay to allow USB CDC ACM to be ready; was 3000ms (watchdog could fire first).
    k_sleep(K_MSEC(500));
    LOG_INF("USB CDC ACM ready");
    Os::init();
    LOG_INF("Os initialized");
    printk("Hello World\n");
    // Initialize the PSRAM

    LOG_INF("Initializing PSRAM in 5 seconds");
    k_sleep(K_MSEC(5000));

    size_t size;
    printk("PSRAM CS pin: %d\n", PSRAM_CS_PIN);
    LOG_INF("PSRAM CS pin: %d", PSRAM_CS_PIN);
    k_sleep(K_MSEC(5000));

    size = sfe_setup_psram(PSRAM_CS_PIN);

    LOG_INF("PSRAM size: %d", size);
    k_sleep(K_MSEC(5000));

    printk("PSRAM size: %d\n", size);
    if (size == 0) {
        printk("Failed to initialize PSRAM\n");
        return 1;
    }
    printk("PSRAM initialized\n");
    LOG_INF("PSRAM initialized");
    printk("PSRAM size: %d\n", size);
    return 0;
}

// Object for communicating state to the topology
//     ReferenceDeployment::TopologyState inputs;
//     // inputs.spi0Device = spi0;

//     // Flight Control Board device bindings
//     inputs.ina219SysDevice = ina219Sys;
//     inputs.ina219SolDevice = ina219Sol;
//     inputs.loraDevice = lora;
//     inputs.uartDevice = serial;
//     inputs.lsm6dsoDevice = lsm6dso;
//     inputs.lis2mdlDevice = lis2mdl;
//     inputs.rtcDevice = rtc;
//     inputs.tca9548aDevice = tca9548a;
//     inputs.muxChannel0Device = mux_channel_0;
//     inputs.muxChannel1Device = mux_channel_1;
//     inputs.muxChannel2Device = mux_channel_2;
//     inputs.muxChannel3Device = mux_channel_3;
//     inputs.muxChannel4Device = mux_channel_4;
//     inputs.muxChannel5Device = mux_channel_5;
//     inputs.muxChannel6Device = mux_channel_6;
//     inputs.muxChannel7Device = mux_channel_7;
//     inputs.storagePartitionId = storage_partition_id;

//     // Face Board device bindings
//     // TMP112 temperature sensor devices
//     inputs.face0TempDevice = face0_temp_sens;
//     inputs.face1TempDevice = face1_temp_sens;
//     inputs.face2TempDevice = face2_temp_sens;
//     inputs.face3TempDevice = face3_temp_sens;
//     inputs.face5TempDevice = face5_temp_sens;
//     inputs.battCell1TempDevice = batt_cell1_temp_sens;
//     inputs.battCell2TempDevice = batt_cell2_temp_sens;
//     inputs.battCell3TempDevice = batt_cell3_temp_sens;
//     inputs.battCell4TempDevice = batt_cell4_temp_sens;
//     // Light sensor devices
//     inputs.face0LightDevice = face0_light_sens;
//     inputs.face1LightDevice = face1_light_sens;
//     inputs.face2LightDevice = face2_light_sens;
//     inputs.face3LightDevice = face3_light_sens;
//     inputs.face5LightDevice = face5_light_sens;
//     inputs.face6LightDevice = face6_light_sens;
//     inputs.face7LightDevice = face7_light_sens;
//     // Magnetorquer devices
//     inputs.face0drv2605Device = face0_drv2605;
//     inputs.face1drv2605Device = face1_drv2605;
//     inputs.face2drv2605Device = face2_drv2605;
//     inputs.face3drv2605Device = face3_drv2605;
//     inputs.face5drv2605Device = face5_drv2605;
//     inputs.baudRate = 115200;

//     // For the uart peripheral config
//     inputs.peripheralBaudRate = 115200;  // Minimum is 19200
//     inputs.peripheralUart = peripheral_uart;
//     inputs.peripheralBaudRate2 = 115200;  // Minimum is 19200
//     inputs.peripheralUart2 = peripheral_uart1;

//     // Setup, cycle, and teardown topology
//     ReferenceDeployment::setupTopology(inputs);
//     ReferenceDeployment::startRateGroups();  // Program loop
//     ReferenceDeployment::teardownTopology(inputs);
//     return 0;
//}
