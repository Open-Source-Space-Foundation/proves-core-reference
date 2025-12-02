// ======================================================================
// \title  SBand.cpp
// \author jrpear
// \brief  cpp file for SBand component implementation class
// ======================================================================

#define RADIOLIB_LOW_LEVEL 1

#include "FprimeZephyrReference/Components/SBand/SBand.hpp"

#include <RadioLib.h>

#include <Fw/Buffer/Buffer.hpp>
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
    Os::ScopeLock lock(this->m_mutex);
    if (this->rx_mode) {
        uint16_t irqStatus = this->m_rlb_radio.getIrqStatus();
        if (irqStatus & RADIOLIB_SX128X_IRQ_RX_DONE) {
            SX1280* radio = &this->m_rlb_radio;
            uint8_t data[256] = {0};
            size_t len = radio->getPacketLength();
            radio->readData(data, len);
            radio->startReceive(RADIOLIB_SX128X_RX_TIMEOUT_INF);

            Fw::Logger::log("MESSAGE RECEIVED:\n");
            char msg[sizeof(data) * 3 + 1];
            for (size_t i = 0; i < len; ++i)
                sprintf(msg + i * 3, "%02X ", data[i]);  // NOLINT(runtime/printf)
            msg[len * 3] = '\0';
            Fw::Logger::log("%s\n", msg);

            // Allocate buffer and send received data to F' system
            // This goes to ComCcsdsSband.frameAccumulator.dataIn, which processes the uplink frames
            // @ jack may want to check here for any edits for how the s band actually processes it
            Fw::Buffer buffer = this->allocate_out(0, static_cast<FwSizeType>(len));
            if (buffer.isValid()) {
                (void)::memcpy(buffer.getData(), data, len);
                ComCfg::FrameContext frameContext;
                this->dataOut_out(0, buffer, frameContext);
            }
        }
    }
}

// ----------------------------------------------------------------------
// Handler implementations for Com interface
// ----------------------------------------------------------------------

void SBand ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    Os::ScopeLock lock(this->m_mutex);
    this->rx_mode = false;

    this->rxEnable_out(0, Fw::Logic::LOW);
    this->txEnable_out(0, Fw::Logic::HIGH);

    int16_t state = this->m_rlb_radio.transmit(data.getData(), data.getSize());
    FW_ASSERT(state == RADIOLIB_ERR_NONE);

    this->enableRx();
    Fw::Success returnStatus = Fw::Success::SUCCESS;
    this->dataReturnOut_out(0, data, context);
    this->comStatusOut_out(0, returnStatus);
}

void SBand ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // Deallocate the buffer
    this->deallocate_out(0, data);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void SBand ::TRANSMIT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->rx_mode = false;  // possible race condition with check in run_handler

    char s[] =
        "Hello, world!\nHello, world!\nHello, world!\nHello, world!\nHello, world!\nHello, world!\nHello, "
        "world!\nHello, world!\nHello, world!\nHello, world!\nHello, world!\nHello, world!\nHello, world!\nHello, "
        "world!\nHello, world!\nHello, world!\nHello, world!\n";
    this->rxEnable_out(0, Fw::Logic::LOW);
    this->txEnable_out(0, Fw::Logic::HIGH);
    int16_t state = this->m_rlb_radio.transmit(s, sizeof(s));
    if (state == RADIOLIB_ERR_NONE) {
        Fw::Logger::log("radio.transmit() success!\n");
    } else {
        Fw::Logger::log("radio.transmit() failed!\n");
        Fw::Logger::log("state: %i\n", state);
    }
    this->enableRx();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// must have mutex held to call this as it touches rx_mode
void SBand ::enableRx() {
    this->txEnable_out(0, Fw::Logic::LOW);
    this->rxEnable_out(0, Fw::Logic::HIGH);

    SX1280* radio = &this->m_rlb_radio;

    int16_t state = radio->standby();
    FW_ASSERT(state == RADIOLIB_ERR_NONE);
    state = radio->startReceive(RADIOLIB_SX128X_RX_TIMEOUT_INF);
    FW_ASSERT(state == RADIOLIB_ERR_NONE);

    this->rx_mode = true;
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

void SBand ::start() {
    Os::ScopeLock lock(this->m_mutex);
    int16_t state = this->configure_radio();
    FW_ASSERT(state == RADIOLIB_ERR_NONE);

    Fw::Success status = Fw::Success::SUCCESS;
    this->comStatusOut_out(0, status);

    this->enableRx();
}

}  // namespace Components
