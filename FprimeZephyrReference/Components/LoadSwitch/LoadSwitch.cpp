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

void LoadSwitch ::GET_IS_ON_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Fw::On state = this->getLoadSwitchState();
    this->log_ACTIVITY_LO_IsOn(state);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

void LoadSwitch ::setLoadSwitchState(Fw::On state) {
    // Check if the state is changing
    if (this->getLoadSwitchState() == state) {
        return;
    }

    // Set the load switch state
    Fw::Logic gpioValue = state ? Fw::Logic::HIGH : Fw::Logic::LOW;
    this->gpioSet_out(0, gpioValue);

    // Inform downstream components of the state change
    for (FwIndexType i = 0; i < this->getNum_loadSwitchStateChanged_OutputPorts(); i++) {
        if (!this->isConnected_loadSwitchStateChanged_OutputPort(i)) {
            continue;
        }
        this->loadSwitchStateChanged_out(i, state);
    }
    this->log_ACTIVITY_HI_StatusChanged(state);
    this->tlmWrite_IsOn(state);
}

Fw::On LoadSwitch ::getLoadSwitchState() {
    Fw::Logic state;
    this->gpioGet_out(0, state);
    return state ? Fw::On::ON : Fw::On::OFF;
}

}  // namespace Components
