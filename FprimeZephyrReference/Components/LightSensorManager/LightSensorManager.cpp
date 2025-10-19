// ======================================================================
// \title  LightSensorManager.cpp
// \author shirleydeng
// \brief  cpp file for LightSensorManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/LightSensorManager/LightSensorManager.hpp"

namespace LightSensor {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

LightSensorManager ::LightSensorManager(const char* const compName) : LightSensorManagerComponentBase(compName) {}

LightSensorManager ::~LightSensorManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Drv::GpioStatus LightSensorManager ::loadSwitch_handler(FwIndexType portNum, Fw::Logic& state) {
    // TODO return
    // Update internal active flag
    this->m_active = (state == Fw::Logic::HIGH);
    return Drv::GpioStatus::OP_OK
}

void LightSensorManager ::run_handler(FwIndexType portNum, U32 context) {
    // TODO
    // TODO: Validate parameters

    // Only perform actions when set to active
    if (this->m_active) {
        // TODO: Toggle states
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void LightSensorManager ::RESET_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // TODO
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace LightSensor
