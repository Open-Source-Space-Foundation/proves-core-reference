// ======================================================================
// \title  LoadSwitch.cpp
// \author Moises, sarah
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

Fw::On LoadSwitch ::loadSwitchStateGet_handler(FwIndexType portNum) {
    return this->getLoadSwitchState() && this->getTime() > this->m_on_timeout ? Fw::On::ON : Fw::On::OFF;
}

void LoadSwitch ::turnOn_handler(FwIndexType portNum) {
    this->setLoadSwitchState(Fw::On::ON);
}

void LoadSwitch ::turnOff_handler(FwIndexType portNum) {
    this->setLoadSwitchState(Fw::On::OFF);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void LoadSwitch ::TURN_ON_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->setLoadSwitchState(Fw::On::ON);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void LoadSwitch ::TURN_OFF_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->setLoadSwitchState(Fw::On::OFF);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

void LoadSwitch ::setLoadSwitchState(Fw::On state) {
    if (this->getLoadSwitchState() == state) {
        // No change, exit early
        return;
    }

    Fw::Logic gpioValue = Fw::Logic::LOW;
    if (state == Fw::On::ON) {
        gpioValue = Fw::Logic::HIGH;
        this->m_on_timeout = this->getTime();
        // Start timeout timer for 1 second
        // This delay is to allow power to stabilize after turning on the load switch
        // TODO(nateinaction): Take delay duration as a parameter
        // TODO(nateinaction): Is there a non-sleep way to determine if load switched board is ready?
        this->m_on_timeout.add(1, 0);
    }

    this->gpioSet_out(0, gpioValue);
    this->log_ACTIVITY_HI_StatusChanged(state);
    this->tlmWrite_IsOn(state);
}

Fw::On LoadSwitch ::getLoadSwitchState() {
    Fw::Logic state;
    this->gpioGet_out(0, state);
    return state ? Fw::On::ON : Fw::On::OFF;
}

}  // namespace Components
