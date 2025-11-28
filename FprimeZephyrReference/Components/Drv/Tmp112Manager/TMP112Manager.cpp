// ======================================================================
// \title  TMP112Manager.cpp
// \brief  cpp file for TMP112Manager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/Tmp112Manager/TMP112Manager.hpp"

#include <Fw/Types/Assert.hpp>

#include <zephyr/sys/printk.h>

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

TMP112Manager ::TMP112Manager(const char* const compName) : TMP112ManagerComponentBase(compName) {}

TMP112Manager ::~TMP112Manager() {}

// ----------------------------------------------------------------------
// Helper methods
// ----------------------------------------------------------------------

void TMP112Manager::configure(const struct device* dev) {
    this->m_dev = dev;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void TMP112Manager ::init_handler(FwIndexType portNum, Fw::Success& condition) {
    int rc = device_init(this->m_dev);
    if (rc < 0) {
        condition = Fw::Success::FAILURE;
        this->log_WARNING_HI_DeviceInitFailed(rc);
    }
    this->log_WARNING_HI_DeviceInitFailed_ThrottleClear();

    condition = Fw::Success::SUCCESS;
}

void TMP112Manager ::deinit_handler(FwIndexType portNum, Fw::Success& condition) {
    if (!this->m_dev || !this->m_dev->state) {
        condition = Fw::Success::FAILURE;
        this->log_WARNING_HI_DeviceDeinitFailed();
        return;
    }

    this->m_dev->state->initialized = false;

    condition = Fw::Success::SUCCESS;
}

F64 TMP112Manager ::temperatureGet_handler(FwIndexType portNum) {
    if (!device_is_ready(this->m_dev)) {
        this->log_WARNING_HI_DeviceNotReady();
        return 0;
    }
    this->log_WARNING_HI_DeviceNotReady_ThrottleClear();

    struct sensor_value temp;

    int rc = sensor_sample_fetch_chan(this->m_dev, SENSOR_CHAN_AMBIENT_TEMP);
    if (rc != 0) {
        this->log_WARNING_HI_SensorFetchFailed(rc);
        return 0;
    }

    rc = sensor_channel_get(this->m_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
    if (rc != 0) {
        this->log_WARNING_HI_SensorChannelGetFailed(rc);
        return 0;
    }

    this->tlmWrite_Temperature(sensor_value_to_double(&temp));

    return sensor_value_to_double(&temp);
}

}  // namespace Drv
