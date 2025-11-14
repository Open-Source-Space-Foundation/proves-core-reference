// ======================================================================
// \title  ResetManager.cpp
// \author nate
// \brief  cpp file for ResetManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/ResetManager/ResetManager.hpp"

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

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

void ResetManager ::handleColdReset() {
    // Log the cold reset event
    this->log_ACTIVITY_HI_INITIATE_COLD_RESET();

    sys_reboot(SYS_REBOOT_COLD);
}

void ResetManager ::handleWarmReset() {
    // Log the warm reset event
    this->log_ACTIVITY_HI_INITIATE_WARM_RESET();

    sys_reboot(SYS_REBOOT_WARM);
}

}  // namespace Components
