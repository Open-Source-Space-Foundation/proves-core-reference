// ======================================================================
// \title  SBand.cpp
// \author jrpear
// \brief  cpp file for SBand component implementation class
// ======================================================================

#define RADIOLIB_LOW_LEVEL 1

#include "FprimeZephyrReference/Components/SBand/SBand.hpp"

#include <RadioLib.h>

#include <Fw/Logger/Logger.hpp>

#include "FprimeHal.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

SBand ::SBand(const char* const compName)
    : SBandComponentBase(compName), m_rlb_hal(this), m_rlb_module(&m_rlb_hal, 0, 5, 0), m_rlb_radio(&m_rlb_module) {}

SBand ::~SBand() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void SBand ::run_handler(FwIndexType portNum, U32 context) {
    if (this->wait_for_rx_fin) {
        uint16_t irqStatus = this->m_rlb_radio.getIrqStatus();
        if (irqStatus & RADIOLIB_SX128X_IRQ_RX_DONE) {
            this->wait_for_rx_fin = false;
            SX1280* radio = &this->m_rlb_radio;
            uint8_t data[256] = {0};
            size_t len = radio->getPacketLength();
            radio->readData(data, len);

            Fw::Logger::log("MESSAGE RECEIVED:\n");
            char msg[sizeof(data) * 3 + 1];
            for (size_t i = 0; i < len; ++i)
                sprintf(msg + i * 3, "%02X ", data[i]);  // NOLINT(runtime/printf)
            msg[len * 3] = '\0';
            Fw::Logger::log("%s\n", msg);
        }
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void SBand ::TRANSMIT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    int state = this->configure_radio();
    FW_ASSERT(state == RADIOLIB_ERR_NONE);

    this->wait_for_rx_fin = false;

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

void SBand ::RECEIVE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->rxEnable_out(0, Fw::Logic::HIGH);

    int16_t state = this->configure_radio();
    FW_ASSERT(state == RADIOLIB_ERR_NONE);

    SX1280* radio = &this->m_rlb_radio;

    state = radio->standby();
    FW_ASSERT(state == RADIOLIB_ERR_NONE);
    state = radio->startReceive(RADIOLIB_SX128X_RX_TIMEOUT_INF);
    FW_ASSERT(state == RADIOLIB_ERR_NONE);

    this->wait_for_rx_fin = true;

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

int16_t SBand ::configure_radio() {
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

}  // namespace Components
