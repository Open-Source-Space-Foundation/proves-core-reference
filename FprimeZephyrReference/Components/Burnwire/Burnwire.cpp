// ======================================================================
// \title  Burnwire.cpp
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
void Burnwire ::burnStart_handler(FwIndexType portNum) {
    this->startBurn();
}

void Burnwire ::burnStop_handler(FwIndexType portNum) {
    this->stopBurn();
}

Fw::Success Burnwire::startBurn() {
    if (this->m_state == Fw::On::ON) {
        this->log_WARNING_HI_BurnwireAlreadyOn();
        return Fw::Success::FAILURE;
    }

    Fw::ParamValid valid;
    U32 timeout = this->paramGet_TIMEOUT(valid);

    if (valid != Fw::ParamValid::VALID) {
        this->log_WARNING_LO_TimeoutParamInvalid();
        return Fw::Success::FAILURE;
    }

    this->m_state = Fw::On::ON;
    this->m_timeoutTime = this->getTime().add(timeout, 0);
    this->gpioSet_out(0, Fw::Logic::HIGH);
    this->gpioSet_out(1, Fw::Logic::HIGH);

    this->log_ACTIVITY_HI_BurnwireState(Fw::On::ON);

    return Fw::Success::SUCCESS;
}

void Burnwire::stopBurn() {
    this->m_state = Fw::On::OFF;
    this->gpioSet_out(0, Fw::Logic::LOW);
    this->gpioSet_out(1, Fw::Logic::LOW);

    this->log_ACTIVITY_HI_BurnwireState(Fw::On::OFF);
}

void Burnwire ::schedIn_handler(FwIndexType portNum, U32 context) {
    if (this->m_state == Fw::On::ON) {
        Fw::Time now = this->getTime();
        if (now >= timeout) {
            stopBurn();
        }
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Burnwire ::START_BURNWIRE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Fw::Success state = this->startBurn();
    if (state != Fw::Success::SUCCESS) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Burnwire ::STOP_BURNWIRE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->stopBurn();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
