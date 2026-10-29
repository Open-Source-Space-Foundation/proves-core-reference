// ======================================================================
// \title  Ina219Manager.cpp
// \brief  cpp file for Ina219Manager component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/Drv/Ina219Manager/Ina219Manager.hpp"

#include <Fw/Types/Assert.hpp>

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Ina219Manager ::Ina219Manager(const char* const compName) : Ina219ManagerComponentBase(compName) {}

Ina219Manager ::~Ina219Manager() {}

// ----------------------------------------------------------------------
// Helper methods
// ----------------------------------------------------------------------

void Ina219Manager::configure(const struct device* dev) {
    this->m_dev = dev;
}

bool Ina219Manager::readChannel(enum sensor_channel chan, struct sensor_value& value) {
    // A failed fetch leaves the driver's last good sample in place. Report 0 instead so that callers
    // such as the mode manager's low-voltage check do not act on a stale reading.
    const int ret = sensor_sample_fetch_chan(this->m_dev, chan);
    if (ret != 0) {
        this->log_WARNING_HI_SampleFetchFailed(ret);
        return false;
    }
    this->log_WARNING_HI_SampleFetchFailed_ThrottleClear();
    return sensor_channel_get(this->m_dev, chan, &value) == 0;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

F64 Ina219Manager ::currentGet_handler(FwIndexType portNum) {
    if (!device_is_ready(this->m_dev)) {
        this->log_WARNING_HI_DeviceNotReady();
        return 0;
    }
    this->log_WARNING_HI_DeviceNotReady_ThrottleClear();

    struct sensor_value current;
    if (!this->readChannel(SENSOR_CHAN_CURRENT, current)) {
        return 0;
    }

    this->tlmWrite_Current(sensor_value_to_double(&current));

    return sensor_value_to_double(&current);
}

F64 Ina219Manager ::powerGet_handler(FwIndexType portNum) {
    if (!device_is_ready(this->m_dev)) {
        this->log_WARNING_HI_DeviceNotReady();
        return 0;
    }
    this->log_WARNING_HI_DeviceNotReady_ThrottleClear();

    struct sensor_value power;
    if (!this->readChannel(SENSOR_CHAN_POWER, power)) {
        return 0;
    }

    this->tlmWrite_Power(sensor_value_to_double(&power));

    return sensor_value_to_double(&power);
}

F64 Ina219Manager ::voltageGet_handler(FwIndexType portNum) {
    if (!device_is_ready(this->m_dev)) {
        this->log_WARNING_HI_DeviceNotReady();
        return 0;
    }
    this->log_WARNING_HI_DeviceNotReady_ThrottleClear();

    struct sensor_value voltage;
    if (!this->readChannel(SENSOR_CHAN_VOLTAGE, voltage)) {
        return 0;
    }

    this->tlmWrite_Voltage(sensor_value_to_double(&voltage));

    return sensor_value_to_double(&voltage);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Ina219Manager::GetVoltage_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    F64 voltage = this->voltageGet_handler(0);
    this->log_ACTIVITY_LO_VoltageReading(voltage);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Ina219Manager::GetCurrent_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    F64 current = this->currentGet_handler(0);
    this->log_ACTIVITY_LO_CurrentReading(current);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Ina219Manager::GetPower_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    F64 power = this->powerGet_handler(0);
    this->log_ACTIVITY_LO_PowerReading(power);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Drv
