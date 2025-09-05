// ======================================================================
// \title  Burnwire.cpp
// \author aldjia
// \brief  cpp file for Burnwire component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Burnwire/Burnwire.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Burnwire ::Burnwire(const char* const compName) : BurnwireComponentBase(compName) {}

Burnwire ::~Burnwire() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void Burnwire ::stop_handler(FwIndexType portNum) {
    // TODO
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Burnwire ::START_BURNWIRE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // update private member variable
    this->m_state = Fw::On::ON;
    // send event
    this->log_ACTIVITY_HI_SetBurnwireState(Fw::On::ON);
    // confirm response
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);

    this->gpioSet_out(0, Fw::Logic::HIGH);
    this->gpioSet_out(1, Fw::Logic::HIGH);
}

void Burnwire ::STOP_BURNWIRE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->m_state = Fw::On::OFF;
    this->log_ACTIVITY_HI_SetBurnwireState(Fw::On::OFF);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    this->gpioSet_out(0, Fw::Logic::LOW);
    this->gpioSet_out(1, Fw::Logic::LOW);
}

}  // namespace Components
