// ======================================================================
// \title  LoadSwitch.cpp
// \author sarah
// \brief  cpp file for LoadSwitch component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/LoadSwitch/LoadSwitch.hpp"
#include <zephyr/drivers/gpio.h>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

LoadSwitch ::LoadSwitch(const char* const compName) : LoadSwitchComponentBase(compName) {}

LoadSwitch ::~LoadSwitch() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void LoadSwitch ::Reset_handler(FwIndexType portNum) {
    this->gpioSet_out(0, Fw::Logic::LOW);
    this->log_ACTIVITY_HI_StatusChanged(Fw::On::OFF);
    this->tlmWrite_IsOn(Fw::On::OFF);
    k_sleep(K_MSEC(100));
    this->gpioSet_out(0, Fw::Logic::HIGH);
    this->log_ACTIVITY_HI_StatusChanged(Fw::On::ON);
    this->tlmWrite_IsOn(Fw::On::ON);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void LoadSwitch ::TURN_ON_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->gpioSet_out(0, Fw::Logic::HIGH);
    this->log_ACTIVITY_HI_StatusChanged(Fw::On::ON);
    this->tlmWrite_IsOn(Fw::On::ON);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);

    this->init_out(0);
}

void LoadSwitch ::TURN_OFF_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->gpioSet_out(0, Fw::Logic::LOW);
    this->log_ACTIVITY_HI_StatusChanged(Fw::On::OFF);
    this->tlmWrite_IsOn(Fw::On::OFF);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
