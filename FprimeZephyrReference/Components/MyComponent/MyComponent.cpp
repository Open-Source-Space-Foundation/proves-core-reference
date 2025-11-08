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
    FwIndexType out_num = getNum_spiSend_OutputPorts();
    U8 write_data[] = {1, 2, 3, 4, 5, 6, 7, 8};
    U8 read_data[8];
    Fw::Buffer writeBuffer(write_data, sizeof(write_data));
    Fw::Buffer readBuffer(read_data, sizeof(read_data));
    this->spiSend_out(out_num, writeBuffer, readBuffer);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
