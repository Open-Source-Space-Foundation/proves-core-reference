// ======================================================================
// \title  Tmp112Manager.cpp
// \brief  cpp file for Tmp112Manager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/Tmp112Manager/Tmp112Manager.hpp"

#include <Fw/Types/Assert.hpp>

#include <zephyr/sys/printk.h>

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Tmp112Manager ::Tmp112Manager(const char* const compName) : Tmp112ManagerComponentBase(compName) {}

Tmp112Manager ::~Tmp112Manager() {}

// ----------------------------------------------------------------------
// Helper methods
// ----------------------------------------------------------------------

void Tmp112Manager::configure(const struct device* dev) {
    this->m_dev = dev;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Fw::Success Tmp112Manager ::loadSwitchStateChanged_handler(FwIndexType portNum, const Fw::On& loadSwitchState) {
    // Store the load switch state
    this->m_load_switch_state = loadSwitchState;

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

F64 Tmp112Manager ::temperatureGet_handler(FwIndexType portNum, Fw::Success& condition) {
    condition = Fw::Success::FAILURE;

    if (!this->initializeDevice()) {
        return 0;
    }

    struct sensor_value val;

    int rc = sensor_sample_fetch_chan(this->m_dev, SENSOR_CHAN_AMBIENT_TEMP);
    if (rc != 0) {
        this->log_WARNING_HI_SensorSampleFetchFailed(rc);
        return 0;
    }
    this->log_WARNING_HI_SensorSampleFetchFailed_ThrottleClear();

    rc = sensor_channel_get(this->m_dev, SENSOR_CHAN_AMBIENT_TEMP, &val);
    if (rc != 0) {
        this->log_WARNING_HI_SensorChannelGetFailed(rc);
        return 0;
    }
    this->log_WARNING_HI_SensorChannelGetFailed_ThrottleClear();

    F64 temp = sensor_value_to_double(&val);
    condition = Fw::Success::SUCCESS;

    this->tlmWrite_Temperature(temp);

    return temp;
}

bool Tmp112Manager ::isDeviceInitialized() {
    if (!this->m_dev) {
        this->log_WARNING_HI_DeviceNil();
        return false;
    }
    this->log_WARNING_HI_DeviceNil_ThrottleClear();

    if (!this->m_dev->state) {
        this->log_WARNING_HI_DeviceStateNil();
        return false;
    }
    this->log_WARNING_HI_DeviceStateNil_ThrottleClear();

    return this->m_dev->state->initialized;
}

Fw::Success Tmp112Manager ::initializeDevice() {
    if (this->isDeviceInitialized()) {
        if (!device_is_ready(this->m_dev)) {
            this->log_WARNING_HI_DeviceNotReady();
            return Fw::Success::FAILURE;
        }
        this->log_WARNING_HI_DeviceNotReady_ThrottleClear();
        return Fw::Success::SUCCESS;
    }

    if (this->tcaHealthGet_out(0) != Fw::Health::HEALTHY) {
        this->log_WARNING_HI_TcaUnhealthy();
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_HI_TcaUnhealthy_ThrottleClear();

    if (this->muxHealthGet_out(0) != Fw::Health::HEALTHY) {
        this->log_WARNING_HI_MuxUnhealthy();
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_HI_MuxUnhealthy_ThrottleClear();

    if (!this->loadSwitchReady()) {
        this->log_WARNING_HI_LoadSwitchNotReady();
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_HI_LoadSwitchNotReady_ThrottleClear();

    int rc = device_init(this->m_dev);
    if (rc < 0) {
        this->log_WARNING_HI_DeviceInitFailed(rc);
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_HI_DeviceInitFailed_ThrottleClear();

    return Fw::Success::SUCCESS;
}

Fw::Success Tmp112Manager ::deinitializeDevice() {
    if (!this->m_dev) {
        this->log_WARNING_HI_DeviceNil();
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_HI_DeviceNil_ThrottleClear();

    if (!this->m_dev->state) {
        this->log_WARNING_HI_DeviceStateNil();
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_HI_DeviceStateNil_ThrottleClear();

    this->m_dev->state->initialized = false;
    return Fw::Success::SUCCESS;
}

bool Tmp112Manager ::loadSwitchReady() {
    return this->m_load_switch_state == Fw::On::ON && this->getTime() >= this->m_load_switch_on_timeout;
}

}  // namespace Drv
