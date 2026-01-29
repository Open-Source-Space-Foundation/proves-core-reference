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
#include <zephyr/drivers/spi.h>

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
// static const struct gpio_dt_spec sbandNrstGpio = GPIO_DT_SPEC_GET(DT_NODELABEL(sband_nrst), gpios);
// static const struct gpio_dt_spec sbandRxEnGpio = GPIO_DT_SPEC_GET(DT_NODELABEL(sband_rx_en), gpios);
// static const struct gpio_dt_spec sbandTxEnGpio = GPIO_DT_SPEC_GET(DT_NODELABEL(sband_tx_en), gpios);
// static const struct gpio_dt_spec sbandTxEnIRQ = GPIO_DT_SPEC_GET(DT_NODELABEL(rf2_io1), gpios);

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
    {getRateGroupPeriod(50), 0},  // 50Hz = 20ms
    {getRateGroupPeriod(10), 0},  // 10Hz = 100ms
    {getRateGroupPeriod(1), 0},   // 1Hz = 1s
}};

// Rate groups may supply a context token to each of the attached children whose purpose is set by the project. The
// reference topology sets each token to zero as these contexts are unused in this project.
U32 rateGroup50HzContext[Svc::ActiveRateGroup::CONNECTION_COUNT_MAX] = {getRateGroupPeriod(50)};
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
    rateGroup50Hz.configure(rateGroup50HzContext, FW_NUM_ARRAY_ELEMENTS(rateGroup50HzContext));
    rateGroup10Hz.configure(rateGroup10HzContext, FW_NUM_ARRAY_ELEMENTS(rateGroup10HzContext));
    rateGroup1Hz.configure(rateGroup1HzContext, FW_NUM_ARRAY_ELEMENTS(rateGroup1HzContext));

    gpioWatchdog.open(ledGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioBurnwire0.open(burnwire0Gpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioBurnwire1.open(burnwire1Gpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);

    cmdSeq.allocateBuffer(0, mallocator, 1024);
    payloadSeq.allocateBuffer(0, mallocator, 1024);
    safeModeSeq.allocateBuffer(0, mallocator, 1024);
    gpioface4LS.open(face4LoadSwitchGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioface0LS.open(face0LoadSwitchGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioface1LS.open(face1LoadSwitchGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioface2LS.open(face2LoadSwitchGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioface3LS.open(face3LoadSwitchGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioface5LS.open(face5LoadSwitchGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioPayloadPowerLS.open(payloadPowerLoadSwitchGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioPayloadBatteryLS.open(payloadBatteryLoadSwitchGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    //    gpioSbandNrst.open(sbandNrstGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    //    gpioSbandRxEn.open(sbandRxEnGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    //    gpioSbandTxEn.open(sbandTxEnGpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    //    gpioSbandIRQ.open(sbandTxEnIRQ, Zephyr::ZephyrGpioDriver::GpioConfiguration::IN);
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

    // static struct spi_cs_control cs_ctrl = {
    //     .gpio = GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(spi0), cs_gpios, 1),
    //     .delay = 0U, /* us to wait after asserting CS before transfer */
    //     .cs_is_gpio = true,
    // };

    // struct spi_config cfg = {
    //     .frequency = 100000,  // 100 KHz -- sx1280 has maximum 18.18 MHz -- there is a 12MHz oscillator on-board
    //     .operation = SPI_WORD_SET(8),
    //     .slave = 0,
    //     .cs = cs_ctrl,
    //     .word_delay = 0,
    // };
    //    spiDriver.configure(state.spi0Device, cfg);
    //    sband.configureRadio();

    // UART from the board to the payload
    peripheralUartDriver.configure(state.peripheralUart, state.peripheralBaudRate);
    imuManager.configure(state.lis2mdlDevice, state.lsm6dsoDevice);
    ina219SysManager.configure(state.ina219SysDevice);
    ina219SolManager.configure(state.ina219SolDevice);

    // Configure camera handlers | NOT ALL SATS HAVE CAMERAS
    cameraHandler.configure(0);  // Camera 0

    // Configure TMP112 temperature sensor managers
    tmp112Face0Manager.configure(state.tca9548aDevice, state.muxChannel0Device, state.face0TempDevice, true);
    tmp112Face1Manager.configure(state.tca9548aDevice, state.muxChannel1Device, state.face1TempDevice, true);
    tmp112Face2Manager.configure(state.tca9548aDevice, state.muxChannel2Device, state.face2TempDevice, true);
    tmp112Face3Manager.configure(state.tca9548aDevice, state.muxChannel3Device, state.face3TempDevice, true);
    tmp112Face5Manager.configure(state.tca9548aDevice, state.muxChannel5Device, state.face5TempDevice, true);
    tmp112BattCell1Manager.configure(state.tca9548aDevice, state.muxChannel4Device, state.battCell1TempDevice, false);
    tmp112BattCell2Manager.configure(state.tca9548aDevice, state.muxChannel4Device, state.battCell2TempDevice, false);
    tmp112BattCell3Manager.configure(state.tca9548aDevice, state.muxChannel4Device, state.battCell3TempDevice, false);
    tmp112BattCell4Manager.configure(state.tca9548aDevice, state.muxChannel4Device, state.battCell4TempDevice, false);

    // Configure VEML6031 light sensor managers
    veml6031Face0Manager.configure(state.tca9548aDevice, state.muxChannel0Device, state.face0LightDevice);
    veml6031Face1Manager.configure(state.tca9548aDevice, state.muxChannel1Device, state.face1LightDevice);
    veml6031Face2Manager.configure(state.tca9548aDevice, state.muxChannel2Device, state.face2LightDevice);
    veml6031Face3Manager.configure(state.tca9548aDevice, state.muxChannel3Device, state.face3LightDevice);
    veml6031Face5Manager.configure(state.tca9548aDevice, state.muxChannel5Device, state.face5LightDevice);
    veml6031Face6Manager.configure(state.tca9548aDevice, state.muxChannel6Device, state.face6LightDevice);
    veml6031Face7Manager.configure(state.tca9548aDevice, state.muxChannel7Device, state.face7LightDevice);

    // Configure DRV2605 magnetorquer managers
    drv2605Face0Manager.configure(state.tca9548aDevice, state.muxChannel0Device, state.face0drv2605Device);
    drv2605Face1Manager.configure(state.tca9548aDevice, state.muxChannel0Device, state.face1drv2605Device);
    drv2605Face2Manager.configure(state.tca9548aDevice, state.muxChannel0Device, state.face2drv2605Device);
    drv2605Face3Manager.configure(state.tca9548aDevice, state.muxChannel0Device, state.face3drv2605Device);
    drv2605Face5Manager.configure(state.tca9548aDevice, state.muxChannel0Device, state.face5drv2605Device);

    detumbleManager.configure();

    fsFormat.configure(state.storagePartitionId);
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
    payloadSeq.deallocateBuffer(mallocator);
    safeModeSeq.deallocateBuffer(mallocator);
}
};  // namespace ReferenceDeployment
