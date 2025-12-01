// ======================================================================
// \title  Velm6031Manager.cpp
// \brief  cpp file for Velm6031Manager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/Velm6031Manager/Velm6031Manager.hpp"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Velm6031Manager ::Velm6031Manager(const char* const compName) : Velm6031ManagerComponentBase(compName) {}

Velm6031Manager ::~Velm6031Manager() {}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

void Velm6031Manager ::configure(const struct device* dev) {
    this->m_dev = dev;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

F32 Velm6031Manager ::ambientLightGet_handler(FwIndexType portNum, Fw::Success& condition) {
    condition = Fw::Success::FAILURE;

    if (!this->initializeDevice()) {
        return 0;
    }

    configureSensorAttributes(SENSOR_CHAN_AMBIENT_LIGHT);  // ignore return value for now

    int rc = sensor_sample_fetch_chan(this->m_dev, SENSOR_CHAN_AMBIENT_LIGHT);
    if (rc != 0) {
        this->log_WARNING_HI_SensorSampleFetchFailed(rc);
        return 0;
    }
    this->log_WARNING_HI_SensorSampleFetchFailed_ThrottleClear();

    struct sensor_value val;
    sensor_channel_get(this->m_dev, SENSOR_CHAN_AMBIENT_LIGHT, &val);

    F32 lux = sensor_value_to_double(&val);

    this->tlmWrite_VisibleLight(lux);

    condition = Fw::Success::SUCCESS;
    return lux;
}

F32 Velm6031Manager ::infraRedLightGet_handler(FwIndexType portNum, Fw::Success& condition) {
    condition = Fw::Success::FAILURE;

    if (!this->initializeDevice()) {
        return 0;
    }

    configureSensorAttributes(SENSOR_CHAN_IR);  // ignore return value for now

    int rc = sensor_sample_fetch_chan(this->m_dev, SENSOR_CHAN_IR);
    if (rc != 0) {
        this->log_WARNING_HI_SensorSampleFetchFailed(rc);
        return 0;
    }
    this->log_WARNING_HI_SensorSampleFetchFailed_ThrottleClear();

    struct sensor_value val;
    sensor_channel_get(this->m_dev, SENSOR_CHAN_IR, &val);

    F32 lux = sensor_value_to_double(&val);

    this->tlmWrite_InfraRedLight(lux);

    condition = Fw::Success::SUCCESS;
    return lux;
}

Fw::Success Velm6031Manager ::loadSwitchStateChanged_handler(FwIndexType portNum, const Fw::On& state) {
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

F32 Velm6031Manager ::visibleLightGet_handler(FwIndexType portNum, Fw::Success& condition) {
    condition = Fw::Success::FAILURE;

    if (!this->initializeDevice()) {
        return 0;
    }

    configureSensorAttributes(SENSOR_CHAN_LIGHT);  // ignore return value for now

    int rc = sensor_sample_fetch_chan(this->m_dev, SENSOR_CHAN_LIGHT);
    if (rc != 0) {
        this->log_WARNING_HI_SensorSampleFetchFailed(rc);
        return 0;
    }
    this->log_WARNING_HI_SensorSampleFetchFailed_ThrottleClear();

    struct sensor_value val;
    sensor_channel_get(this->m_dev, SENSOR_CHAN_LIGHT, &val);

    F32 lux = sensor_value_to_double(&val);

    this->tlmWrite_VisibleLight(lux);

    condition = Fw::Success::SUCCESS;
    return lux;
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

bool Velm6031Manager ::isDeviceInitialized() {
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

Fw::Success Velm6031Manager ::initializeDevice() {
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

Fw::Success Velm6031Manager ::deinitializeDevice() {
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

bool Velm6031Manager ::loadSwitchReady() {
    return this->m_load_switch_state == Fw::On::ON && this->getTime() >= this->m_load_switch_on_timeout;
}

Fw::Success Velm6031Manager ::setIntegrationTimeAttribute(sensor_channel chan) {
    Fw::ParamValid valid;
    U8 it = this->paramGet_INTEGRATION_TIME(valid);
    if (valid != Fw::ParamValid::VALID) {
        this->log_WARNING_HI_InvalidIntegrationTimeParam(it);
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_HI_InvalidIntegrationTimeParam_ThrottleClear();

    struct sensor_value attr;
    sensor_value_from_double(&attr, it);
    int rc = sensor_attr_set(this->m_dev, chan, (enum sensor_attribute)SENSOR_ATTR_VEML6031_IT, &attr);
    if (rc) {
        this->log_WARNING_HI_SensorAttrSetFailed(SENSOR_ATTR_VEML6031_IT, it, rc);
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_HI_SensorAttrSetFailed_ThrottleClear();

    return Fw::Success::SUCCESS;
}

Fw::Success Velm6031Manager ::setGainAttribute(sensor_channel chan) {
    Fw::ParamValid valid;
    U8 gain = this->paramGet_GAIN(valid);
    if (valid != Fw::ParamValid::VALID) {
        this->log_WARNING_HI_InvalidGainParam(gain);
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_HI_InvalidGainParam_ThrottleClear();

    struct sensor_value attr;
    sensor_value_from_double(&attr, gain);
    int rc = sensor_attr_set(this->m_dev, chan, (enum sensor_attribute)SENSOR_ATTR_VEML6031_GAIN, &attr);
    if (rc) {
        this->log_WARNING_HI_SensorAttrSetFailed(SENSOR_ATTR_VEML6031_GAIN, gain, rc);
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_HI_SensorAttrSetFailed_ThrottleClear();

    return Fw::Success::SUCCESS;
}

Fw::Success Velm6031Manager ::setDiv4Attribute(sensor_channel chan) {
    Fw::ParamValid valid;
    U8 div4 = this->paramGet_EFFECTIVE_PHOTODIODE_SIZE(valid);
    if (valid != Fw::ParamValid::VALID) {
        this->log_WARNING_HI_InvalidDiv4Param(div4);
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_HI_InvalidDiv4Param_ThrottleClear();

    struct sensor_value attr;
    sensor_value_from_double(&attr, div4);
    int rc = sensor_attr_set(this->m_dev, chan, (enum sensor_attribute)SENSOR_ATTR_VEML6031_DIV4, &attr);
    if (rc) {
        this->log_WARNING_HI_SensorAttrSetFailed(SENSOR_ATTR_VEML6031_DIV4, div4, rc);
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_HI_SensorAttrSetFailed_ThrottleClear();

    return Fw::Success::SUCCESS;
}

Fw::Success Velm6031Manager ::configureSensorAttributes(sensor_channel chan) {
    Fw::Success result;

    result = this->setIntegrationTimeAttribute(chan);
    if (result != Fw::Success::SUCCESS) {
        return result;
    }

    result = this->setGainAttribute(chan);
    if (result != Fw::Success::SUCCESS) {
        return result;
    }

    result = this->setDiv4Attribute(chan);
    if (result != Fw::Success::SUCCESS) {
        return result;
    }

    return Fw::Success::SUCCESS;
}

}  // namespace Drv
