// ======================================================================
// \title  Burnwire.cpp
// \brief  cpp file for Burnwire component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Burnwire/Burnwire.hpp"

#include <algorithm>
#include <limits>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Burnwire ::Burnwire(const char* const compName) : BurnwireComponentBase(compName) {
    this->m_safetyCounter = 0;
    this->m_state = Fw::On::OFF;
    this->m_timerRunning = false;
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

void Burnwire::startBurn() {
    this->log_ACTIVITY_HI_SetBurnwireState(Fw::On::ON);
    this->m_safetyCounter = 0;
    this->m_state = Fw::On::ON;

    this->m_burnTimer.start();
    this->m_timerRunning = true;

    Fw::ParamValid valid;
    U32 timeout = this->paramGet_SAFETY_TIMER(valid);
    this->log_ACTIVITY_HI_SafetyTimerState(timeout);
}

void Burnwire::stopBurn() {
    this->log_ACTIVITY_HI_SetBurnwireState(Fw::On::OFF);
    this->gpioSet_out(0, Fw::Logic::LOW);
    this->gpioSet_out(1, Fw::Logic::LOW);

    const U32 countedSeconds = this->m_safetyCounter.load();

    U32 elapsedSeconds = countedSeconds;
    if (this->m_timerRunning) {
        this->m_burnTimer.stop();
        const U32 elapsedUsec = this->m_burnTimer.getDiffUsec();

        if (elapsedUsec != std::numeric_limits<U32>::max()) {
            const U32 roundedSeconds = static_cast<U32>((elapsedUsec + 500000U) / 1000000U);
            elapsedSeconds = std::max(elapsedSeconds, roundedSeconds);
        }

        this->m_timerRunning = false;
    }

    this->m_state = Fw::On::OFF;
    this->log_ACTIVITY_LO_BurnwireEndCount(elapsedSeconds);
}

void Burnwire ::schedIn_handler(FwIndexType portNum, U32 context) {
    Fw::ParamValid valid;
    U32 timeout = this->paramGet_SAFETY_TIMER(valid);

    if (this->m_state == Fw::On::ON) {
        this->m_safetyCounter++;
        if (this->m_safetyCounter == 1) {
            this->gpioSet_out(0, Fw::Logic::HIGH);
            this->gpioSet_out(1, Fw::Logic::HIGH);
            this->log_ACTIVITY_HI_SafetyTimerStatus(Fw::On::ON);
        }

        if (this->m_safetyCounter >= timeout) {
            stopBurn();
            this->log_ACTIVITY_HI_SafetyTimerStatus(Fw::On::OFF);
        }
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Burnwire ::START_BURNWIRE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->startBurn();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Burnwire ::STOP_BURNWIRE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->stopBurn();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
