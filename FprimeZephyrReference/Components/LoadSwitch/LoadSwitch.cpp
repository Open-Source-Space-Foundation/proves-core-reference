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

void LoadSwitch ::Reset_handler(FwIndexType portNum) {
    this->setLoadSwitchState(Fw::On::OFF);
    k_sleep(K_MSEC(100));
    this->setLoadSwitchState(Fw::On::ON);
}

Fw::On LoadSwitch ::loadSwitchStateGet_handler(FwIndexType portNum) {
    // TODO(nateinaction): Delay reporting state change until power has normalized
    Fw::Logic state;
    this->gpioGet_out(0, state);
    return state ? Fw::On::ON : Fw::On::OFF;
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
    Fw::Logic gpioValue = (state == Fw::On::ON) ? Fw::Logic::HIGH : Fw::Logic::LOW;
    this->gpioSet_out(0, gpioValue);
    this->log_ACTIVITY_HI_StatusChanged(state);
    this->tlmWrite_IsOn(state);
}

}  // namespace Components
