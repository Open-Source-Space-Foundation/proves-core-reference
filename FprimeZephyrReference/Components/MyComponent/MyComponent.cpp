// ======================================================================
// \title  MyComponent.cpp
// \author jrpear
// \brief  cpp file for MyComponent component implementation class
// ======================================================================

#define RADIOLIB_LOW_LEVEL 1

#include "FprimeZephyrReference/Components/MyComponent/MyComponent.hpp"

#include <RadioLib.h>

#include <Fw/Logger/Logger.hpp>

#include "FprimeHal.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

MyComponent ::MyComponent(const char* const compName)
    : MyComponentComponentBase(compName),
      m_rlb_hal(this),
      m_rlb_module(&m_rlb_hal, 0, 5, 0),
      m_rlb_radio(&m_rlb_module) {}

MyComponent ::~MyComponent() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void MyComponent ::run_handler(FwIndexType portNum, U32 context) {
    // TODO
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void MyComponent ::TRANSMIT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    int state = this->configure_radio();
    FW_ASSERT(state == RADIOLIB_ERR_NONE);
    char s[] =
        "Hello, world!\nHello, world!\nHello, world!\nHello, world!\nHello, world!\nHello, world!\nHello, "
        "world!\nHello, world!\nHello, world!\nHello, world!\nHello, world!\nHello, world!\nHello, world!\nHello, "
        "world!\nHello, world!\nHello, world!\nHello, world!\n";
    this->txEnable_out(0, Fw::Logic::HIGH);
    state = this->m_rlb_radio.transmit(s, sizeof(s));
    this->txEnable_out(0, Fw::Logic::LOW);
    if (state == RADIOLIB_ERR_NONE) {
        Fw::Logger::log("radio.transmit() success!\n");
    } else {
        Fw::Logger::log("radio.transmit() failed!\n");
        Fw::Logger::log("state: %i\n", state);
    }
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void MyComponent ::RECEIVE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->rxEnable_out(0, Fw::Logic::HIGH);

    int state = this->configure_radio();
    FW_ASSERT(state == RADIOLIB_ERR_NONE);

    uint8_t buf[256] = {0};

    // cannot specify timeout greater than 2^16 * 15.625us = 1024 ms as timeout
    // is internally resolved to 16-bit representation of 15.625us step count
    state = this->m_rlb_radio.receive(buf, sizeof(buf), RadioLibTime_t(1024 * 1000));
    if (state == RADIOLIB_ERR_NONE) {
        Fw::Logger::log("radio.receive() success!\n");
    } else {
        Fw::Logger::log("radio.receive() failed!\n");
        Fw::Logger::log("state: %i\n", state);
    }

    Fw::Logger::log("RESULTING BUFFER:\n");

    char msg[sizeof(buf) * 3 + 1];

    for (size_t i = 0; i < sizeof(buf); ++i) {
        sprintf(msg + i * 3, "%02X ", buf[i]);  // NOLINT(runtime/printf)
    }
    msg[sizeof(buf) * 3] = '\0';

    Fw::Logger::log("%s\n", msg);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void MyComponent ::READ_DATA_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    uint8_t buf[256] = {0};

    int16_t state = this->m_rlb_radio.readData(buf, sizeof(buf));
    if (state == RADIOLIB_ERR_NONE) {
        Fw::Logger::log("radio.readData() success!\n");
    } else {
        Fw::Logger::log("radio.readData() failed!\n");
        Fw::Logger::log("state: %i\n", state);
    }

    Fw::Logger::log("readData() buffer:\n");

    char msg[sizeof(buf) * 3 + 1];

    for (size_t i = 0; i < sizeof(buf); ++i) {
        sprintf(msg + i * 3, "%02X ", buf[i]);  // NOLINT(runtime/printf)
    }
    msg[sizeof(buf) * 3] = '\0';

    Fw::Logger::log("%s\n", msg);

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

int16_t MyComponent ::configure_radio() {
    int state = this->m_rlb_radio.begin();
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }

    state = this->m_rlb_radio.setOutputPower(13);  // 13dB is max
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }

    // Match modulation parameters to CircuitPython defaults
    state = this->m_rlb_radio.setSpreadingFactor(7);
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }

    state = this->m_rlb_radio.setBandwidth(406.25);
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }

    state = this->m_rlb_radio.setCodingRate(5);
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }

    state = this->m_rlb_radio.setPacketParamsLoRa(12, RADIOLIB_SX128X_LORA_HEADER_EXPLICIT, 255,
                                                  RADIOLIB_SX128X_LORA_CRC_ON, RADIOLIB_SX128X_LORA_IQ_STANDARD);
    return state;
}

void MyComponent ::RESET_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // BROKEN
    this->resetSend_out(0, Fw::Logic::HIGH);
    Os::Task::delay(Fw::TimeInterval(0, 1000));
    this->resetSend_out(0, Fw::Logic::LOW);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
