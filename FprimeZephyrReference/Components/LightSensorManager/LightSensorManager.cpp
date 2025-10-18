// ======================================================================
// \title  LightSensorManager.cpp
// \author adriano
// \brief  cpp file for LightSensorManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/LightSensorManager/LightSensorManager.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

LightSensorManager ::LightSensorManager(const char* const compName) : LightSensorManagerComponentBase(compName) {}

LightSensorManager ::~LightSensorManager() {}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void LightSensorManager ::RESET_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // TODO
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
