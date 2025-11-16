// ======================================================================
// \title  Watchdog.cpp
// \author moisesmata
// \brief  cpp file for Watchdog component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Watchdog/Watchdog.hpp"

#include "config/FpConfig.hpp"
#include <zephyr/drivers/hwinfo.h>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Watchdog ::Watchdog(const char* const compName) : WatchdogComponentBase(compName) {}

Watchdog ::~Watchdog() {}

void Watchdog ::preamble() {
    // Check if the system was reset by watchdog timeout
    uint32_t reset_cause = 0;
    int ret = hwinfo_get_reset_cause(&reset_cause);

    if (ret == 0 && (reset_cause & RESET_WATCHDOG)) {
        // Watchdog timeout caused this boot - signal fault to ModeManager
        if (this->isConnected_watchdogFault_OutputPort(0)) {
            this->watchdogFault_out(0);  // Signal port - no parameters
        }
        this->log_ACTIVITY_HI_WatchdogStart();  // Log that watchdog fault was detected
    }

    // Clear reset cause after reading so it doesn't persist to next boot
    hwinfo_clear_reset_cause();
}

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
    // call start handler
    this->start_handler(0);

    // Provide command response
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Watchdog ::STOP_WATCHDOG_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // call stop handler
    this->stop_handler(0);

    // Provide command response
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
