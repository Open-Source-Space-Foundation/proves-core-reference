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

#include <zephyr/drivers/gpio.h>

static const struct gpio_dt_spec ledGpio = GPIO_DT_SPEC_GET(DT_NODELABEL(led0), gpios);
static const struct gpio_dt_spec burnwire0Gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(burnwire0), gpios);
static const struct gpio_dt_spec burnwire1Gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(burnwire1), gpios);

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
    // Rate group driver needs a divisor list
    rateGroupDriver.configure(rateGroupDivisorsSet);
    // Rate groups require context arrays.
    rateGroup10Hz.configure(rateGroup10HzContext, FW_NUM_ARRAY_ELEMENTS(rateGroup10HzContext));
    rateGroup1Hz.configure(rateGroup1HzContext, FW_NUM_ARRAY_ELEMENTS(rateGroup1HzContext));

    gpioDriver.open(ledGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioBurnwire0.open(burnwire0Gpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioBurnwire1.open(burnwire1Gpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
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
    // Autocoded parameter loading. Function provided by autocoder.
    loadParameters();
    // Autocoded task kick-off (active components). Function provided by autocoder.
    startTasks(state);

    // Uplink is configured for receive so a socket task is started
    comDriver.configure(state.uartDevice, state.baudRate);
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
}
};  // namespace ReferenceDeployment
