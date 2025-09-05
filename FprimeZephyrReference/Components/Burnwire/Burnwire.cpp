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

// void Burnwire ::stop_handler(FwIndexType portNum) {
//     //TODO
// }

void Burnwire ::schedIn_handler(FwIndexType portNum, U32 context) {
    if (this->m_state == Fw::On::ON) {
        this->m_safetyCounter++;
        if (this->m_safetyCounter == 1) {
            this->gpioSet_out(0, Fw::Logic::HIGH);
            this->gpioSet_out(1, Fw::Logic::HIGH);
            this->log_ACTIVITY_HI_SafetyTimerStatus(Fw::On::ON);
        }

        if (this->m_safetyCounter >= m_safetyMaxCount) {
            // 30 seconds reached → turn OFF
            this->gpioSet_out(0, Fw::Logic::LOW);
            this->gpioSet_out(1, Fw::Logic::LOW);

            this->m_state = Fw::On::OFF;
            this->log_ACTIVITY_HI_SetBurnwireState(Fw::On::OFF);
            this->log_ACTIVITY_HI_SafetyTimerStatus(Fw::On::OFF);
        }
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Burnwire ::START_BURNWIRE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // reset count to 0
    this->m_safetyCounter = 0;
    // update private member variable
    this->m_state = Fw::On::ON;
    // send event
    this->log_ACTIVITY_HI_SetBurnwireState(Fw::On::ON);
    // confirm response
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Burnwire ::STOP_BURNWIRE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->m_state = Fw::On::OFF;
    this->log_ACTIVITY_HI_SetBurnwireState(Fw::On::OFF);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    this->gpioSet_out(0, Fw::Logic::LOW);
    this->gpioSet_out(1, Fw::Logic::LOW);
}

}  // namespace Components
