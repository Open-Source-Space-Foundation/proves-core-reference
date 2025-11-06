// ======================================================================
// \title  MyComponent.cpp
// \author jrpear
// \brief  cpp file for MyComponent component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/MyComponent/MyComponent.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

MyComponent ::MyComponent(const char* const compName) : MyComponentComponentBase(compName) {}

MyComponent ::~MyComponent() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void MyComponent ::run_handler(FwIndexType portNum, U32 context) {
    // TODO
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void MyComponent ::FOO_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // TODO
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
