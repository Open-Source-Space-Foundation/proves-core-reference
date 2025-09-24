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

Burnwire ::Burnwire(const char* const compName) : BurnwireComponentBase(compName) {
    this->m_safetyCounter = 0;
    this->m_state = Fw::On::OFF;
    Fw::ParamValid valid;
    this->m_safetyMaxCount = this->paramGet_SAFETY_TIMER(valid);
    this->log_ACTIVITY_HI_SafetyTimerSet(this->m_safetyMaxCount);
    this->log_ACTIVITY_HI_SafetyTimerSet(m_safetyMaxCount);
}

Burnwire ::~Burnwire() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------
void Burnwire ::burnStart_handler(FwIndexType portNum) {
    this->startBurn();
}

void Burnwire ::burnStop_handler(FwIndexType portNum) {
    this->stopBurn();
}

// void Burnwire ::stop_handler(FwIndexType portNum) {
//     //TODO
// }

void Burnwire::startBurn() {
    this->m_safetyCounter = 0;
    this->m_state = Fw::On::ON;
    this->log_ACTIVITY_HI_SetBurnwireState(Fw::On::ON);
    this->log_ACTIVITY_HI_SafetyTimerSet(this->m_safetyMaxCount);
}

void Burnwire::stopBurn() {
    this->m_state = Fw::On::OFF;
    this->log_ACTIVITY_HI_SetBurnwireState(Fw::On::OFF);
    this->gpioSet_out(0, Fw::Logic::LOW);
    this->gpioSet_out(1, Fw::Logic::LOW);
}

void Burnwire ::schedIn_handler(FwIndexType portNum, U32 context) {
    if (this->m_state == Fw::On::ON) {
        this->m_safetyCounter++;
        if (this->m_safetyCounter == 1) {
            this->gpioSet_out(0, Fw::Logic::HIGH);
            this->gpioSet_out(1, Fw::Logic::HIGH);
            this->log_ACTIVITY_HI_SafetyTimerStatus(Fw::On::ON);
        }

        if (this->m_safetyCounter >= this->m_safetyMaxCount) {
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

void Burnwire ::START_BURNWIRE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 max_duration) {
    this->m_safetyMaxCount = max_duration;
    this->startBurn();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Burnwire ::STOP_BURNWIRE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->stopBurn();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Burnwire ::parameterUpdated(FwPrmIdType id) {
    if (id == this->PARAMID_SAFETY_TIMER) {
        Fw::ParamValid valid;
        this->m_safetyMaxCount = this->paramGet_SAFETY_TIMER(valid);
        this->log_ACTIVITY_HI_SafetyTimerSet(this->m_safetyMaxCount);
    }
    this->log_ACTIVITY_HI_SafetyTimerSet(m_safetyMaxCount);
}

}  // namespace Components
