// ======================================================================
// \title  ResetManager.cpp
// \author nate
// \brief  cpp file for ResetManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/ResetManager/ResetManager.hpp"

#include <Os/Task.hpp>

#include <zephyr/sys/reboot.h>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ResetManager ::ResetManager(const char* const compName) : ResetManagerComponentBase(compName) {}

ResetManager ::~ResetManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void ResetManager ::coldReset_handler(FwIndexType portNum) {
    this->handleColdReset();
}

void ResetManager ::warmReset_handler(FwIndexType portNum) {
    this->handleWarmReset();
}

void ResetManager ::radioReset_handler(FwIndexType portNum) {
    this->handleRadioReset();
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void ResetManager ::COLD_RESET_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->handleColdReset();

    // The command response will not be received since the system is resetting
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
}

void ResetManager ::WARM_RESET_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->handleWarmReset();

    // The command response will not be received since the system is resetting
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
}

void ResetManager ::RESET_RADIO_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->handleRadioReset();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

void ResetManager ::handleColdReset() {
    // Log the cold reset event
    this->log_ACTIVITY_HI_INITIATE_COLD_RESET();

    // Notify ModeManager to set clean shutdown flag before rebooting
    // This allows ModeManager to detect unintended reboots on next startup
    if (this->isConnected_prepareForReboot_OutputPort(0)) {
        this->prepareForReboot_out(0);
    }

    sys_reboot(SYS_REBOOT_COLD);
}

void ResetManager ::handleWarmReset() {
    // Log the warm reset event
    this->log_ACTIVITY_HI_INITIATE_WARM_RESET();

    // Notify ModeManager to set clean shutdown flag before rebooting
    // This allows ModeManager to detect unintended reboots on next startup
    if (this->isConnected_prepareForReboot_OutputPort(0)) {
        this->prepareForReboot_out(0);
    }

    sys_reboot(SYS_REBOOT_WARM);
}

void ResetManager ::handleRadioReset() {
    // Log the radio reset event
    this->log_ACTIVITY_HI_INITIATE_RADIO_RESET();

    // Only toggle GPIO if port is connected (allows testing without hardware)
    if (this->isConnected_radioResetOut_OutputPort(0)) {
        // Pull radio reset line LOW (active low reset)
        this->radioResetOut_out(0, Fw::Logic::LOW);

        // Hold reset for minimum duration (50ms is safe for most radio modules)
        Os::Task::delay(Fw::TimeInterval(0, 50000));  // 0 seconds, 50000 microseconds = 50ms

        // Release reset line HIGH
        this->radioResetOut_out(0, Fw::Logic::HIGH);

        // Allow radio time to reinitialize (100ms typical)
        Os::Task::delay(Fw::TimeInterval(0, 100000));  // 0 seconds, 100000 microseconds = 100ms
    }
}

}  // namespace Components
