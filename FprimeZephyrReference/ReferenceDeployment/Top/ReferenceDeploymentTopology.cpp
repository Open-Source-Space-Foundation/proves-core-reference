// ======================================================================
// \title  ReferenceDeploymentTopology.cpp
// \brief cpp file containing the topology instantiation code
//
// ======================================================================
// Provides access to autocoded functions
#include <FprimeZephyrReference/ReferenceDeployment/Top/ReferenceDeploymentTopologyAc.hpp>
// Note: Uncomment when using Svc:TlmPacketizer
// #include <FprimeZephyrReference/ReferenceDeployment/Top/ReferenceDeploymentPacketsAc.hpp>

// Necessary project-specified types
#include <Fw/Types/MallocAllocator.hpp>
#include <Os/FileSystem.hpp>

#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

static const struct gpio_dt_spec ledGpio = GPIO_DT_SPEC_GET(DT_NODELABEL(led0), gpios);
static const struct gpio_dt_spec burnwire0Gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(burnwire0), gpios);
static const struct gpio_dt_spec burnwire1Gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(burnwire1), gpios);
static const struct gpio_dt_spec face0LoadSwitchGpio = GPIO_DT_SPEC_GET(DT_NODELABEL(face0_enable), gpios);
static const struct gpio_dt_spec face1LoadSwitchGpio = GPIO_DT_SPEC_GET(DT_NODELABEL(face1_enable), gpios);
static const struct gpio_dt_spec face2LoadSwitchGpio = GPIO_DT_SPEC_GET(DT_NODELABEL(face2_enable), gpios);
static const struct gpio_dt_spec face3LoadSwitchGpio = GPIO_DT_SPEC_GET(DT_NODELABEL(face3_enable), gpios);
static const struct gpio_dt_spec face4LoadSwitchGpio = GPIO_DT_SPEC_GET(DT_NODELABEL(face4_enable), gpios);
static const struct gpio_dt_spec face5LoadSwitchGpio = GPIO_DT_SPEC_GET(DT_NODELABEL(face5_enable), gpios);
static const struct gpio_dt_spec payloadPowerLoadSwitchGpio = GPIO_DT_SPEC_GET(DT_NODELABEL(payload_pwr_enable), gpios);
static const struct gpio_dt_spec payloadBatteryLoadSwitchGpio =
    GPIO_DT_SPEC_GET(DT_NODELABEL(payload_batt_enable), gpios);

// Allows easy reference to objects in FPP/autocoder required namespaces
using namespace ReferenceDeployment;

// Instantiate a malloc allocator for cmdSeq buffer allocation
Fw::MallocAllocator mallocator;

constexpr FwSizeType BASE_RATEGROUP_PERIOD_MS = 1;  // 1Khz

// Helper function to calculate the period for a given rate group frequency
constexpr FwSizeType getRateGroupPeriod(const FwSizeType hz) {
    return 1000 / (hz * BASE_RATEGROUP_PERIOD_MS);
}

// The reference topology divides the incoming clock signal (1Hz) into sub-signals: 1Hz, 1/2Hz, and 1/4Hz with 0 offset
Svc::RateGroupDriver::DividerSet rateGroupDivisorsSet{{
    // Array of divider objects
    {getRateGroupPeriod(10), 0},  // 10Hz
    {getRateGroupPeriod(1), 0},   // 1Hz
}};

// Rate groups may supply a context token to each of the attached children whose purpose is set by the project. The
// reference topology sets each token to zero as these contexts are unused in this project.
U32 rateGroup10HzContext[Svc::ActiveRateGroup::CONNECTION_COUNT_MAX] = {getRateGroupPeriod(10)};
U32 rateGroup1HzContext[Svc::ActiveRateGroup::CONNECTION_COUNT_MAX] = {getRateGroupPeriod(1)};

/**
 * \brief configure/setup components in project-specific way
 *
 * This is a *helper* function which configures/sets up each component requiring project specific input. This includes
 * allocating resources, passing-in arguments, etc. This function may be inlined into the topology setup function if
 * desired, but is extracted here for clarity.
 */
void configureTopology() {
    FileHandling::prmDb.configure("/prmDb.dat");
    // Rate group driver needs a divisor list
    rateGroupDriver.configure(rateGroupDivisorsSet);
    // Rate groups require context arrays.
    rateGroup10Hz.configure(rateGroup10HzContext, FW_NUM_ARRAY_ELEMENTS(rateGroup10HzContext));
    rateGroup1Hz.configure(rateGroup1HzContext, FW_NUM_ARRAY_ELEMENTS(rateGroup1HzContext));

    gpioWatchdog.open(ledGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioBurnwire0.open(burnwire0Gpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioBurnwire1.open(burnwire1Gpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);

    cmdSeq.allocateBuffer(0, mallocator, 5 * 1024);
    gpioface4LS.open(face4LoadSwitchGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioface0LS.open(face0LoadSwitchGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioface1LS.open(face1LoadSwitchGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioface2LS.open(face2LoadSwitchGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioface3LS.open(face3LoadSwitchGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioface5LS.open(face5LoadSwitchGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioPayloadPowerLS.open(payloadPowerLoadSwitchGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioPayloadBatteryLS.open(payloadBatteryLoadSwitchGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
}

// Public functions for use in main program are namespaced with deployment name ReferenceDeployment
namespace ReferenceDeployment {
void setupTopology(const TopologyState& state) {

    // Autocoded initialization. Function provided by autocoder.
    initComponents(state);
    // Autocoded id setup. Function provided by autocoder.
    setBaseIds();
    // Autocoded connection wiring. Function provided by autocoder.
    connectComponents();
    // Autocoded command registration. Function provided by autocoder.
    regCommands();
    // Autocoded configuration. Function provided by autocoder.
    configComponents(state);
    // Project-specific component configuration. Function provided above. May be inlined, if desired.
    configureTopology();
    // Read parameters from persistent storage
    readParameters();
    // Autocoded parameter loading. Function provided by autocoder.
    loadParameters();
    // Autocoded task kick-off (active components). Function provided by autocoder.
    startTasks(state);

    // Try to configure the RTC device first because all other components need time
    rtcManager.configure(state.rtcDevice);

    // We have a pipeline for both the LoRa and UART drive to allow for ground harness debugging an
    // for over-the-air communications.
    lora.start(state.loraDevice, Zephyr::TransmitState::DISABLED);
    comDriver.configure(state.uartDevice, state.baudRate);

    // printk("initializing log file... \n");
    TlmLoggerTee::comLog.init_log_file("/Tlm", 1024 * 30, true);
    EventLoggerTee::comLog.init_log_file("/Event", 1024 * 30, true);
    // printk("log file initialized\n");
    
    lsm6dsoManager.configure(state.lsm6dsoDevice);
    lis2mdlManager.configure(state.lis2mdlDevice);
    ina219SysManager.configure(state.ina219SysDevice);
    ina219SolManager.configure(state.ina219SolDevice);
}

void startRateGroups() {
    timer.configure(BASE_RATEGROUP_PERIOD_MS);
    timer.start();
    while (1) {
        timer.cycle();
    }
}

void stopRateGroups() {
    timer.stop();
}

void teardownTopology(const TopologyState& state) {
    // Autocoded (active component) task clean-up. Functions provided by topology autocoder.
    stopTasks(state);
    freeThreads(state);
    tearDownComponents(state);
    cmdSeq.deallocateBuffer(mallocator);
}
};  // namespace ReferenceDeployment
