// ======================================================================
// \title  Main.cpp
// \brief main program for the F' application. Intended for CLI-based systems (Linux, macOS)
//
// ======================================================================
// Used to access topology functions

#include <FprimeZephyrReference/ReferenceDeployment/Top/ReferenceDeploymentTopology.hpp>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>

const struct device* ina219Sys = device_get_binding("INA219 sys");
const struct device* ina219Sol = device_get_binding("INA219 sol");
const struct device* serial = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));
const struct device* lora = DEVICE_DT_GET(DT_NODELABEL(lora0));
const struct device* lsm6dso = DEVICE_DT_GET(DT_NODELABEL(lsm6dso0));
const struct device* lis2mdl = DEVICE_DT_GET(DT_NODELABEL(lis2mdl0));
const struct device* mcp23017 = DEVICE_DT_GET(DT_NODELABEL(mcp23017));

// Magnetorquer devices
const struct device* face0_drv2605 = device_get_binding("FACE0_DRV2605");
const struct device* face1_drv2605 = device_get_binding("FACE1_DRV2605");
const struct device* face2_drv2605 = device_get_binding("FACE2_DRV2605");
const struct device* face3_drv2605 = device_get_binding("FACE3_DRV2605");
const struct device* face4_drv2605 = device_get_binding("FACE4_DRV2605");
const struct device* face5_drv2605 = device_get_binding("FACE5_DRV2605");

/**
 * @brief Early boot initialization function to power on load switches
 *
 * This function runs during system initialization BEFORE sensor devices initialize.
 * It powers on all face board and battery board load switches to ensure TMP112
 * sensors have power when their init functions run.
 *
 * Priority: 75 (after I2C mux channels at 71, before sensors at 90)
 */
static int proves_board_power_init(void) {
    const struct device* mcp23017_dev = DEVICE_DT_GET(DT_NODELABEL(mcp23017));

    if (!device_is_ready(mcp23017_dev)) {
        printk("ERROR: MCP23017 not ready during early boot init!\n");
        return -1;
    }

    printk("[Early Init] Powering on all face boards and battery board...\n");

    // Turn on all 6 face board load switches
    gpio_pin_configure(mcp23017_dev, 8, GPIO_OUTPUT_ACTIVE);  // face4
    gpio_pin_configure(mcp23017_dev, 9, GPIO_OUTPUT_ACTIVE);  // face0
    gpio_pin_configure(mcp23017_dev, 10, GPIO_OUTPUT_ACTIVE); // face1
    gpio_pin_configure(mcp23017_dev, 11, GPIO_OUTPUT_ACTIVE); // face2
    gpio_pin_configure(mcp23017_dev, 12, GPIO_OUTPUT_ACTIVE); // face3
    gpio_pin_configure(mcp23017_dev, 13, GPIO_OUTPUT_ACTIVE); // face5

    // Turn on battery board load switch
    gpio_pin_configure(mcp23017_dev, 3, GPIO_OUTPUT_ACTIVE);  // battery

    // Allow 100ms for power to stabilize before sensor init
    k_sleep(K_MSEC(100));

    printk("[Early Init] All boards powered on, ready for sensor initialization\n");

    return 0;
}

// Register early init function to run after I2C mux (71) but before sensors (90)
SYS_INIT(proves_board_power_init, POST_KERNEL, 75);

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

    // TMP112 temperature sensor devices
    inputs.face0TempDevice = DEVICE_DT_GET(DT_NODELABEL(face0_temp_sens));
    inputs.face1TempDevice = DEVICE_DT_GET(DT_NODELABEL(face1_temp_sens));
    inputs.face2TempDevice = DEVICE_DT_GET(DT_NODELABEL(face2_temp_sens));
    inputs.face3TempDevice = DEVICE_DT_GET(DT_NODELABEL(face3_temp_sens));
    inputs.face4TempDevice = DEVICE_DT_GET(DT_NODELABEL(face4_temp_sens));
    inputs.face5TempDevice = DEVICE_DT_GET(DT_NODELABEL(face5_temp_sens));
    inputs.topTempDevice = DEVICE_DT_GET(DT_NODELABEL(top_temp_sens));
    inputs.battCell1TempDevice = DEVICE_DT_GET(DT_NODELABEL(batt_cell1_temp_sens));
    inputs.battCell2TempDevice = DEVICE_DT_GET(DT_NODELABEL(batt_cell2_temp_sens));
    inputs.battCell3TempDevice = DEVICE_DT_GET(DT_NODELABEL(batt_cell3_temp_sens));
    inputs.battCell4TempDevice = DEVICE_DT_GET(DT_NODELABEL(batt_cell4_temp_sens));

    // Diagnostic: Check which TMP112 sensors are ready
    printk("\n=== TMP112 Sensor Ready Status ===\n");
    const struct device* sensors[] = {
        inputs.face0TempDevice, inputs.face1TempDevice,
        inputs.face2TempDevice, inputs.face3TempDevice,
        inputs.face4TempDevice, inputs.face5TempDevice,
        inputs.topTempDevice,
        inputs.battCell1TempDevice, inputs.battCell2TempDevice,
        inputs.battCell3TempDevice, inputs.battCell4TempDevice
    };
    const char* names[] = {"face0", "face1", "face2", "face3",
                           "face4", "face5", "top",
                           "batt1", "batt2", "batt3", "batt4"};
    for(int i=0; i<11; i++) {
        int ready = device_is_ready(sensors[i]);
        printk("  %s: %s (ptr: %p)\n", names[i], ready ? "READY" : "NOT READY", sensors[i]);
    }
    printk("==================================\n\n");

    // Magnetorquer devices
    inputs.drv2605Devices[0] = face0_drv2605;
    inputs.drv2605Devices[1] = face1_drv2605;
    inputs.drv2605Devices[2] = face2_drv2605;
    inputs.drv2605Devices[3] = face3_drv2605;
    inputs.drv2605Devices[4] = face4_drv2605;
    inputs.drv2605Devices[5] = face5_drv2605;

    inputs.baudRate = 115200;

    // Setup, cycle, and teardown topology
    ReferenceDeployment::setupTopology(inputs);
    ReferenceDeployment::startRateGroups();  // Program loop
    ReferenceDeployment::teardownTopology(inputs);
    return 0;
}
