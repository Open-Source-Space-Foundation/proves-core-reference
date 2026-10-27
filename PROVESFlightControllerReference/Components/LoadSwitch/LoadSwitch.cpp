// ======================================================================
// \title  LoadSwitch.cpp
// \author Moises, sarah
// \brief  cpp file for LoadSwitch component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/LoadSwitch/LoadSwitch.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

LoadSwitch::LoadSwitch(const char* const compName) : LoadSwitchComponentBase(compName) {}

LoadSwitch::~LoadSwitch() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void LoadSwitch::turnOn_handler(FwIndexType portNum) {
    this->setLoadSwitchState(Fw::On::ON);
}

void LoadSwitch::turnOff_handler(FwIndexType portNum) {
    this->setLoadSwitchState(Fw::On::OFF);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void LoadSwitch::TURN_ON_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    const bool ok = this->setLoadSwitchState(Fw::On::ON);
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

void LoadSwitch::TURN_OFF_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    const bool ok = this->setLoadSwitchState(Fw::On::OFF);
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

void LoadSwitch::GET_IS_ON_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Fw::On state;
    if (!this->readLoadSwitchState(state)) {
        this->log_WARNING_HI_ReadFailed();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    this->log_ACTIVITY_LO_IsOn(state);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

bool LoadSwitch::setLoadSwitchState(Fw::On state) {
    // Write unless the switch is known to be in the requested state. A failed read (e.g. the
    // GPIO expander is unreachable on a wedged bus) must still attempt the write.
    Fw::On current;
    const bool switched = this->readLoadSwitchState(current) && current == state;
    if (switched && this->m_notifiedState == state) {
        return true;
    }

    // Set the GPIO high before notification on ON
    if (state == Fw::On::ON && !switched && !this->writeLoadSwitchState(state)) {
        this->log_WARNING_HI_SwitchFailed(state);
        return false;
    }

    // Inform downstream components of the state change. On OFF this happens even if the
    // write below fails, so they stop talking to a device that may be misbehaving.
    for (FwIndexType i = 0; i < this->getNum_loadSwitchStateChanged_OutputPorts(); i++) {
        if (!this->isConnected_loadSwitchStateChanged_OutputPort(i)) {
            continue;
        }
        this->loadSwitchStateChanged_out(i, state);
    }
    this->m_notifiedState = state;

    // Set the GPIO low after notification on OFF
    if (state == Fw::On::OFF && !switched && !this->writeLoadSwitchState(state)) {
        this->log_WARNING_HI_SwitchFailed(state);
        return false;
    }
    this->log_ACTIVITY_HI_StatusChanged(state);
    this->tlmWrite_IsOn(state);
    return true;
}

bool LoadSwitch::writeLoadSwitchState(Fw::On state) {
    static constexpr U8 MAX_ATTEMPTS = 3;
    const Fw::Logic level = (state == Fw::On::ON) ? Fw::Logic::HIGH : Fw::Logic::LOW;
    for (U8 attempt = 0; attempt < MAX_ATTEMPTS; attempt++) {
        Fw::On readBack;
        if (this->gpioSet_out(0, level) == Drv::GpioStatus::OP_OK && this->readLoadSwitchState(readBack) &&
            readBack == state) {
            return true;
        }
    }
    return false;
}

bool LoadSwitch::readLoadSwitchState(Fw::On& state) {
    Fw::Logic level;
    if (this->gpioGet_out(0, level) != Drv::GpioStatus::OP_OK) {
        return false;
    }
    state = (level == Fw::Logic::HIGH) ? Fw::On::ON : Fw::On::OFF;
    return true;
}

}  // namespace Components
