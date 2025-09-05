// ======================================================================
// \title  INA219Manager.cpp
// \author ncc-michael
// \brief  cpp file for INA219Manager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/INA219Manager/INA219Manager.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

INA219Manager ::INA219Manager(const char* const compName) : INA219ManagerComponentBase(compName) {}

INA219Manager ::~INA219Manager() {}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void INA219Manager ::TODO_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // TODO
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
