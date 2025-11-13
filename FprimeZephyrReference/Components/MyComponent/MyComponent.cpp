// ======================================================================
// \title  MyComponent.cpp
// \author jrpear
// \brief  cpp file for MyComponent component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/MyComponent/MyComponent.hpp"

#include <Fw/Logger/Logger.hpp>

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
    FwIndexType out_idx = 0;
    // Firmware version is at 0x153 -- expect 0xB7A9 or 0xB5A9
    //                 OP    ADDR_HI  ADDR_LOW NOP NOP NOP
    /*U8 write_data[] = {0x19, 0x1,     0x53,    0,  0,  0};*/
    U8 write_data[] = {0xC0, 0};
    U8 read_data[sizeof(write_data)];
    Fw::Buffer writeBuffer(write_data, sizeof(write_data));
    Fw::Buffer readBuffer(read_data, sizeof(read_data));
    this->spiSend_out(out_idx, writeBuffer, readBuffer);
    for (int i = 0; i < sizeof(read_data); i++) {
        Fw::Logger::log("Byte %i: %" PRI_U8 "\n", i, read_data[i]);
    }
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void MyComponent ::RESET_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // UNTESTED
    this->resetSend_out(0, Fw::Logic::LOW);
    /*this->gpioSet_out(0, (Fw::On::ON == this->m_state) ? Fw::Logic::HIGH : Fw::Logic::LOW);*/
    Os::Task::delay(Fw::TimeInterval(0, 1000));
    this->resetSend_out(0, Fw::Logic::HIGH);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
