// ======================================================================
// \title  Drv2605Manager.cpp
// \author aychar
// \brief  cpp file for Drv2605Manager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/Drv2605Manager/Drv2605Manager.hpp"

#include <zephyr/device.h>
#include <zephyr/drivers/haptics.h>
#include <zephyr/drivers/haptics/drv2605.h>

namespace Drv {

static uint8_t input_arr[1];
static struct drv2605_rtp_data rtp = {
    .size = 1,
    .rtp_hold_us = (uint32_t[]){1},
    .rtp_input = input_arr,
};

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Drv2605Manager ::Drv2605Manager(const char* const compName) : Drv2605ManagerComponentBase(compName) {}

Drv2605Manager ::~Drv2605Manager() {}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

void Drv2605Manager::configure(const struct device* tca, const struct device* mux, const struct device* dev) {
    this->m_tca = tca;
    this->m_mux = mux;
    this->m_dev = dev;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Fw::Success Drv2605Manager ::loadSwitchStateChanged_handler(FwIndexType portNum, const Fw::On& state) {
    // Store the load switch state
    this->m_load_switch_state = state;

    // If the load switch is off, deinitialize the device
    if (this->m_load_switch_state == Fw::On::OFF) {
        return this->deinitializeDevice();
    }

    // If the load switch is on, set the timeout
    // We only consider the load switch to be fully on after a timeout period
    this->m_load_switch_on_timeout = this->getTime();
    this->m_load_switch_on_timeout.add(1, 0);

    return Fw::Success::SUCCESS;
}

Fw::Success Drv2605Manager ::start_handler(FwIndexType portNum, I8 val) {
    if (!this->initializeDevice()) {
        return Fw::Success::FAILURE;
    }

    // Set the RTP data
    input_arr[0] = static_cast<uint8_t>(val);
    union drv2605_config_data config_data;
    config_data.rtp_data = &rtp;

    int rc = drv2605_haptic_config(this->m_dev, DRV2605_HAPTICS_SOURCE_RTP, &config_data);
    if (rc < 0) {
        this->log_WARNING_LO_DeviceHapticConfigSetFailed(rc);
        return Fw::Success::FAILURE;
    }

    // Start the magnetorquer
    rc = haptics_start_output(this->m_dev);
    if (rc < 0) {
        this->log_WARNING_LO_TriggerFailed(rc);
        return Fw::Success::FAILURE;
    }
    return Fw::Success::SUCCESS;
}

Fw::Success Drv2605Manager ::stop_handler(FwIndexType portNum) {
    if (this->start_handler(0, 0) != Fw::Success::SUCCESS) {
        return Fw::Success::FAILURE;
    }
    return Fw::Success::SUCCESS;
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Drv2605Manager ::START_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, I8 val) {
    // Trigger the magnetorquer
    Fw::Success condition = this->start_handler(0, val);
    if (condition != Fw::Success::SUCCESS) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Drv2605Manager ::STOP_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Stop the magnetorquer
    this->stop_handler(0);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

bool Drv2605Manager ::isDeviceInitialized() {
    if (!this->m_dev) {
        this->log_WARNING_LO_DeviceNil();
        return false;
    }
    this->log_WARNING_LO_DeviceNil_ThrottleClear();

    if (!this->m_dev->state) {
        this->log_WARNING_LO_DeviceStateNil();
        return false;
    }
    this->log_WARNING_LO_DeviceStateNil_ThrottleClear();

    return this->m_dev->state->initialized;
}

Fw::Success Drv2605Manager ::initializeDevice() {
    if (this->isDeviceInitialized()) {
        if (!device_is_ready(this->m_dev)) {
            this->log_WARNING_LO_DeviceNotReady();
            return Fw::Success::FAILURE;
        }
        this->log_WARNING_LO_DeviceNotReady_ThrottleClear();
        return Fw::Success::SUCCESS;
    }

    if (!device_is_ready(this->m_tca)) {
        this->log_WARNING_LO_TcaUnhealthy();
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_LO_TcaUnhealthy_ThrottleClear();

    if (!device_is_ready(this->m_mux)) {
        this->log_WARNING_LO_MuxUnhealthy();
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_LO_MuxUnhealthy_ThrottleClear();

    if (!this->loadSwitchReady()) {
        return Fw::Success::FAILURE;
    }

    int rc = device_init(this->m_dev);
    if (rc < 0) {
        // Log the initialization failure
        this->log_WARNING_LO_DeviceInitFailed(rc);

        // Deinitialize the device to reset state
        this->deinitializeDevice();
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_LO_DeviceInitFailed_ThrottleClear();

    this->log_ACTIVITY_LO_DeviceInitialized();

    return Fw::Success::SUCCESS;
}

Fw::Success Drv2605Manager ::deinitializeDevice() {
    if (!this->m_dev) {
        this->log_WARNING_LO_DeviceNil();
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_LO_DeviceNil_ThrottleClear();

    if (!this->m_dev->state) {
        this->log_WARNING_LO_DeviceStateNil();
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_LO_DeviceStateNil_ThrottleClear();

    this->m_dev->state->initialized = false;
    this->m_dev->state->init_res = 0;
    return Fw::Success::SUCCESS;
}

bool Drv2605Manager ::loadSwitchReady() {
    return this->m_load_switch_state == Fw::On::ON && this->getTime() >= this->m_load_switch_on_timeout;
}

}  // namespace Drv
