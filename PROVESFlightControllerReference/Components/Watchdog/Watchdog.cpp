// ======================================================================
// \title  Watchdog.cpp
// \author moisesmata
// \brief  cpp file for Watchdog component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/Watchdog/Watchdog.hpp"

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

    // Write initial telemetry value to ensure it's available immediately
    this->tlmWrite_WatchdogTransitions(this->m_transitions);

    // Report watchdog started
    this->log_ACTIVITY_HI_WatchdogStart();
}

void Watchdog ::stop_handler(FwIndexType portNum) {
    // Stopping the watchdog leads to a hardware reset once petting ceases, so this
    // IS the planned-reboot notification point for every stop path (ground command
    // via STOP_WATCHDOG and ModeManager's safe-mode stopWatchdog port alike).
    for (FwIndexType i = 0; i < this->getNum_prepareForReboot_OutputPorts(); i++) {
        if (this->isConnected_prepareForReboot_OutputPort(i)) {
            this->prepareForReboot_out(i);
        }
    }

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
    // call stop handler (which fans out prepareForReboot to all listeners)
    this->stop_handler(0);
    // Provide command response
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
