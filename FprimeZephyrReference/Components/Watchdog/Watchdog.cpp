// ======================================================================
// \title  Watchdog.cpp
// \author moisesmata
// \brief  cpp file for Watchdog component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Watchdog/Watchdog.hpp"
#include "config/FpConfig.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Watchdog ::Watchdog(const char* const compName) : WatchdogComponentBase(compName) {}

Watchdog ::~Watchdog() {}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

void Watchdog ::run_handler(FwIndexType portNum, U32 context) {
    // Only perform actions when run is enabled
    if (this->m_run) {
        // Toggle state every rate group call
        this->m_state = (this->m_state == Fw::On::ON) ? Fw::On::OFF : Fw::On::ON;
        this->m_transitions++;
        this->tlmWrite_WatchdogTransitions(this->m_transitions);

        this->gpioSet_out(0, (Fw::On::ON == this->m_state) ? Fw::Logic::HIGH : Fw::Logic::LOW);
    }
}

void Watchdog ::start_handler(FwIndexType portNum) {
    // Start the watchdog
    this->m_run = true;

    // Report watchdog started
    this->log_ACTIVITY_HI_WatchdogStart();
}

void Watchdog ::stop_handler(FwIndexType portNum) {
    // Stop the watchdog
    this->m_run = false;

    // Report watchdog stopped
    this->log_ACTIVITY_HI_WatchdogStop();
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Watchdog ::START_WATCHDOG_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);

    // call start handler
    this->start_handler(0);

    // Provide command response
}

void Watchdog ::STOP_WATCHDOG_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);

    // call stop handler
    this->stop_handler(0);

    // Provide command response
}

}  // namespace Components
