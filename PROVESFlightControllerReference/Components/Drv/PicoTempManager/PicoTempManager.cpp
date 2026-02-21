// ======================================================================
// \title  PicoTempManager.cpp
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
    Fw::Success condition = Fw::Success::FAILURE;
    F64 temperature = this->getPicoTemperature(condition);
    if (condition != Fw::Success::SUCCESS) {
        return;
    }
    this->tlmWrite_PicoTemperature(temperature);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void PicoTempManager ::GetPicoTemperature_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Fw::Success condition = Fw::Success::FAILURE;
    F64 temperature = this->getPicoTemperature(condition);
    if (condition != Fw::Success::SUCCESS) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    this->log_ACTIVITY_HI_PicoTemperature(temperature);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

F64 PicoTempManager ::getPicoTemperature(Fw::Success& condition) {
    if (!device_is_ready(this->m_dev)) {
        this->log_WARNING_LO_DeviceNotReady();
        return 0;
    }
    this->log_WARNING_LO_DeviceNotReady_ThrottleClear();

    int rc = sensor_sample_fetch(this->m_dev);
    if (rc != 0) {
        this->log_WARNING_LO_SensorSampleFetchFailed(rc);
        return 0;
    }
    this->log_WARNING_LO_SensorSampleFetchFailed_ThrottleClear();

    struct sensor_value temp_val;
    rc = sensor_channel_get(this->m_dev, SENSOR_CHAN_DIE_TEMP, &temp_val);
    if (rc != 0) {
        this->log_WARNING_LO_SensorChannelGetFailed(rc);
        return 0.0;
    }
    this->log_WARNING_LO_SensorChannelGetFailed_ThrottleClear();
    F64 temp = sensor_value_to_double(&temp_val);
    condition = Fw::Success::SUCCESS;
    return temp;
}

}  // namespace Drv
