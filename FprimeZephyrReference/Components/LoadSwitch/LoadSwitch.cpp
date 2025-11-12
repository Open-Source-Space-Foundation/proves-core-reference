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
    gpio_pin_set(m_device, m_pinNum, 0);
    k_sleep(K_MSEC(100));
    gpio_pin_set(m_device, m_pinNum, 1);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void LoadSwitch ::TURN_ON_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    gpio_pin_set(m_device, m_pinNum, 1);
    this->log_ACTIVITY_HI_StatusChanged(Fw::On::ON);
    // I think some code needed to send this status to the port as well
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void LoadSwitch ::TURN_OFF_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    gpio_pin_set(m_device, m_pinNum, 0);
    this->log_ACTIVITY_HI_StatusChanged(Fw::On::OFF);
    // I think some code needed to send this status to the port as well
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// This is meant to be used in Topology.cpp to configure the pin and device
void LoadSwitch ::pin_configuration(const struct device* device, uint8_t pinNum) {
    this->m_pinNum = pinNum;
    this->m_device = device;
    gpio_pin_configure(m_device, m_pinNum, GPIO_OUTPUT_ACTIVE);
}

}  // namespace Components
