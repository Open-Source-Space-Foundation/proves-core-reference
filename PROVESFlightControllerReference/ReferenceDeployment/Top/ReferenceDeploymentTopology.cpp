// ======================================================================
// \title  ReferenceDeploymentTopology.cpp
// \brief cpp file containing the topology instantiation code
//
// ======================================================================
// Provides access to autocoded functions
#include <PROVESFlightControllerReference/ReferenceDeployment/Top/ReferenceDeploymentTopologyAc.hpp>
// Note: Uncomment when using Svc:TlmPacketizer
// #include <PROVESFlightControllerReference/ReferenceDeployment/Top/ReferenceDeploymentPacketsAc.hpp>

// Necessary project-specified types
#include <Fw/Types/MallocAllocator.hpp>
#include <zephyr/sys/printk.h>

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
    {getRateGroupPeriod(1), 0},   // 1Hz
}};

// Rate groups may supply a context token to each of the attached children whose purpose is set by the project. The
// reference topology sets each token to zero as these contexts are unused in this project.
U32 rateGroup1HzContext[Svc::ActiveRateGroup::CONNECTION_COUNT_MAX] = {};

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
    rateGroup1Hz.configure(rateGroup1HzContext, FW_NUM_ARRAY_ELEMENTS(rateGroup1HzContext));
}

// Public functions for use in main program are namespaced with deployment name ReferenceDeployment
namespace ReferenceDeployment {
void setupTopology(const TopologyState& state) {
    printk("[SETUP] initComponents\n");
    initComponents(state);
    printk("[SETUP] setBaseIds\n");
    setBaseIds();
    printk("[SETUP] connectComponents\n");
    connectComponents();
    printk("[SETUP] regCommands\n");
    regCommands();
    printk("[SETUP] configComponents\n");
    configComponents(state);
    printk("[SETUP] configureTopology\n");
    configureTopology();
    printk("[SETUP] loadParameters\n");
    loadParameters();
    printk("[SETUP] startTasks\n");
    startTasks(state);
    printk("[SETUP] comDriver.configure\n");
    comDriver.configure(state.uartDevice, state.baudRate);
    printk("[SETUP] complete\n");
}

void startRateGroups() {
    timer.configure(BASE_RATEGROUP_PERIOD_MS);
    printk("\n=== Rate groups starting (period=%u ms) ===\n", static_cast<unsigned int>(BASE_RATEGROUP_PERIOD_MS));
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
