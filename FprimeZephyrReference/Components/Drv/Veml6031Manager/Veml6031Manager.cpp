// ======================================================================
// \title  Veml6031Manager.cpp
// \brief  cpp file for Veml6031Manager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/Veml6031Manager/Veml6031Manager.hpp"

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Veml6031Manager ::Veml6031Manager(const char* const compName) : Veml6031ManagerComponentBase(compName) {}

Veml6031Manager ::~Veml6031Manager() {}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

void Veml6031Manager ::configure(const struct device* tca, const struct device* mux, const struct device* dev) {
    this->m_tca = tca;
    this->m_mux = mux;
    this->m_dev = dev;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

F32 Veml6031Manager ::ambientLightGet_handler(FwIndexType portNum, Fw::Success& condition) {
    condition = Fw::Success::FAILURE;

    if (!this->initializeDevice()) {
        return 0;
    }

    configureSensorAttributes(SENSOR_CHAN_AMBIENT_LIGHT);  // ignore return value for now

    int rc = sensor_sample_fetch_chan(this->m_dev, SENSOR_CHAN_AMBIENT_LIGHT);
    if (rc != 0) {
        this->log_WARNING_LO_SensorSampleFetchFailed(rc);
        return 0;
    }
    this->log_WARNING_LO_SensorSampleFetchFailed_ThrottleClear();

    struct sensor_value val;
    rc = sensor_channel_get(this->m_dev, SENSOR_CHAN_AMBIENT_LIGHT, &val);
    if (rc != 0) {
        this->log_WARNING_LO_SensorChannelGetFailed(rc);
        return 0;
    }
    this->log_WARNING_LO_SensorChannelGetFailed_ThrottleClear();

    F32 lux = sensor_value_to_double(&val);

    this->tlmWrite_AmbientLight(lux);

    condition = Fw::Success::SUCCESS;
    return lux;
}

F32 Veml6031Manager ::infraRedLightGet_handler(FwIndexType portNum, Fw::Success& condition) {
    condition = Fw::Success::FAILURE;

    if (!this->initializeDevice()) {
        return 0;
    }

    configureSensorAttributes(SENSOR_CHAN_IR);  // ignore return value for now

    int rc = sensor_sample_fetch_chan(this->m_dev, SENSOR_CHAN_IR);
    if (rc != 0) {
        this->log_WARNING_LO_SensorSampleFetchFailed(rc);
        return 0;
    }
    this->log_WARNING_LO_SensorSampleFetchFailed_ThrottleClear();

    struct sensor_value val;
    rc = sensor_channel_get(this->m_dev, SENSOR_CHAN_IR, &val);
    if (rc != 0) {
        this->log_WARNING_LO_SensorChannelGetFailed(rc);
        return 0;
    }
    this->log_WARNING_LO_SensorChannelGetFailed_ThrottleClear();

    F32 lux = sensor_value_to_double(&val);

    this->tlmWrite_InfraRedLight(lux);

    condition = Fw::Success::SUCCESS;
    return lux;
}

Fw::Success Veml6031Manager ::loadSwitchStateChanged_handler(FwIndexType portNum, const Fw::On& state) {
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

F32 Veml6031Manager ::visibleLightGet_handler(FwIndexType portNum, Fw::Success& condition) {
    condition = Fw::Success::FAILURE;

    if (!this->initializeDevice()) {
        return 0;
    }

    configureSensorAttributes(SENSOR_CHAN_LIGHT);  // ignore return value for now

    int rc = sensor_sample_fetch_chan(this->m_dev, SENSOR_CHAN_LIGHT);
    if (rc != 0) {
        this->log_WARNING_LO_SensorSampleFetchFailed(rc);
        return 0;
    }
    this->log_WARNING_LO_SensorSampleFetchFailed_ThrottleClear();

    struct sensor_value val;
    rc = sensor_channel_get(this->m_dev, SENSOR_CHAN_LIGHT, &val);
    if (rc != 0) {
        this->log_WARNING_LO_SensorChannelGetFailed(rc);
        return 0;
    }
    this->log_WARNING_LO_SensorChannelGetFailed_ThrottleClear();

    F32 lux = sensor_value_to_double(&val);

    this->tlmWrite_VisibleLight(lux);

    condition = Fw::Success::SUCCESS;
    return lux;
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Veml6031Manager ::GetVisibleLight_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Fw::Success condition;
    F32 lux = this->visibleLightGet_handler(0, condition);
    if (condition != Fw::Success::SUCCESS) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    this->log_ACTIVITY_HI_VisibleLight(lux);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Veml6031Manager ::GetInfraRedLight_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Fw::Success condition;
    F32 lux = this->infraRedLightGet_handler(0, condition);
    if (condition != Fw::Success::SUCCESS) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    this->log_ACTIVITY_HI_InfraRedLight(lux);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Veml6031Manager ::GetAmbientLight_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Fw::Success condition;
    F32 lux = this->ambientLightGet_handler(0, condition);
    if (condition != Fw::Success::SUCCESS) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    this->log_ACTIVITY_HI_AmbientLight(lux);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

bool Veml6031Manager ::isDeviceInitialized() {
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

Fw::Success Veml6031Manager ::initializeDevice() {
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
        this->log_WARNING_LO_LoadSwitchNotReady();
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_LO_LoadSwitchNotReady_ThrottleClear();

    int rc = device_init(this->m_dev);
    if (rc < 0) {
        this->log_WARNING_LO_DeviceInitFailed(rc);
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_LO_DeviceInitFailed_ThrottleClear();

    return Fw::Success::SUCCESS;
}

Fw::Success Veml6031Manager ::deinitializeDevice() {
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
    return Fw::Success::SUCCESS;
}

bool Veml6031Manager ::loadSwitchReady() {
    return this->m_load_switch_state == Fw::On::ON && this->getTime() >= this->m_load_switch_on_timeout;
}

Fw::Success Veml6031Manager ::setIntegrationTimeAttribute(sensor_channel chan) {
    Fw::ParamValid valid;
    U8 it = this->paramGet_INTEGRATION_TIME(valid);
    if (valid != Fw::ParamValid::VALID) {
        this->log_WARNING_LO_InvalidIntegrationTimeParam(it);
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_LO_InvalidIntegrationTimeParam_ThrottleClear();

    struct sensor_value val;
    // val.val1 = it;
    // val.val2 = 0;
    sensor_value_from_double(&val, it);
    int rc = sensor_attr_set(this->m_dev, chan, (enum sensor_attribute)SENSOR_ATTR_VEML6031_IT, &val);
    if (rc) {
        Fw::LogStringArg attr("SENSOR_ATTR_VEML6031_IT");
        this->log_WARNING_LO_SensorAttrSetFailed(attr, it, rc);
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_LO_SensorAttrSetFailed_ThrottleClear();

    return Fw::Success::SUCCESS;
}

Fw::Success Veml6031Manager ::setGainAttribute(sensor_channel chan) {
    Fw::ParamValid valid;
    U8 gain = this->paramGet_GAIN(valid);
    if (valid != Fw::ParamValid::VALID) {
        this->log_WARNING_LO_InvalidGainParam(gain);
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_LO_InvalidGainParam_ThrottleClear();

    struct sensor_value val;
    sensor_value_from_double(&val, gain);
    // val.val1 = gain;
    // val.val2 = 0;
    int rc = sensor_attr_set(this->m_dev, chan, (enum sensor_attribute)SENSOR_ATTR_VEML6031_GAIN, &val);
    if (rc) {
        Fw::LogStringArg attr("SENSOR_ATTR_VEML6031_GAIN");
        this->log_WARNING_LO_SensorAttrSetFailed(attr, gain, rc);
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_LO_SensorAttrSetFailed_ThrottleClear();

    return Fw::Success::SUCCESS;
}

Fw::Success Veml6031Manager ::setDiv4Attribute(sensor_channel chan) {
    Fw::ParamValid valid;
    U8 div4 = this->paramGet_EFFECTIVE_PHOTODIODE_SIZE(valid);
    if (valid != Fw::ParamValid::VALID) {
        this->log_WARNING_LO_InvalidDiv4Param(div4);
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_LO_InvalidDiv4Param_ThrottleClear();

    struct sensor_value val;
    sensor_value_from_double(&val, div4);
    // val.val1 = div4;
    // val.val2 = 0;
    int rc = sensor_attr_set(this->m_dev, chan, (enum sensor_attribute)SENSOR_ATTR_VEML6031_DIV4, &val);
    if (rc) {
        Fw::LogStringArg attr("SENSOR_ATTR_VEML6031_DIV4");
        this->log_WARNING_LO_SensorAttrSetFailed(attr, div4, rc);
        return Fw::Success::FAILURE;
    }
    this->log_WARNING_LO_SensorAttrSetFailed_ThrottleClear();

    return Fw::Success::SUCCESS;
}

Fw::Success Veml6031Manager ::configureSensorAttributes(sensor_channel chan) {
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
