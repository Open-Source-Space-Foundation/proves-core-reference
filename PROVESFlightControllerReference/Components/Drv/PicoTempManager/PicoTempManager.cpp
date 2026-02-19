// ======================================================================
// \title  PicoTempManager.cpp
// \author jcowley04
// \brief  cpp file for PicoTempManager component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/Drv/PicoTempManager/PicoTempManager.hpp"

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PicoTempManager ::PicoTempManager(const char* const compName) : PicoTempManagerComponentBase(compName) {}

PicoTempManager ::~PicoTempManager() {}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------
void PicoTempManager::configure(const struct device* dev) {
    this->m_dev = dev;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void PicoTempManager ::run_handler(FwIndexType portNum, U32 context) {
    Fw::Success condition;
    F64 temperature = this->getPicoTemperature(condition);
    if (condition != Fw::Success::SUCCESS) {
        this->log_WARNING_LO_TemperatureReadFailed();
        return;
    }
    this->log_WARNING_LO_TemperatureReadFailed_ThrottleClear();

    this->tlmWrite_Temperature(temperature);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void PicoTempManager ::GetPicoTemperature_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Fw::Success condition;
    F64 temperature = this->getPicoTemperature(condition);
    if (condition != Fw::Success::SUCCESS) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    this->log_ACTIVITY_HI_Temperature(temperature);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

bool PicoTempManager ::isDeviceInitialized() {
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

Fw::Success PicoTempManager ::initializeDevice() {
    if (this->isDeviceInitialized()) {
        if (!device_is_ready(this->m_dev)) {
            this->log_WARNING_LO_DeviceNotReady();
            return Fw::Success::FAILURE;
        }
        this->log_WARNING_LO_DeviceNotReady_ThrottleClear();
        return Fw::Success::SUCCESS;
    }
    int rc = device_init(this->m_dev);
    if (rc < 0) {
        this->log_WARNING_LO_DeviceInitFailed(rc);
        this->deinitializeDevice();
        return Fw::Success::FAILURE;
    }
}

F64 PicoTempManager ::getPicoTemperature(Fw::Success& condition) {
    if (!this->isDeviceInitialized()) {
        condition = Fw::Success::FAILURE;
        return 0.0;
    }
    struct sensor_value temp_val;
    int rc = sensor_channel_get(this->m_dev, SENSOR_CHAN_DIE_TEMP, &temp_val);
    if (rc < 0) {
        this->log_WARNING_LO_TemperatureReadFailed(rc);
        condition = Fw::Success::FAILURE;
        return 0.0;
    }
    F64 temp = sensor_value_to_double(&temp_val);
    condition = Fw::Success::SUCCESS;
    return temp;
}

// namespace Drv
