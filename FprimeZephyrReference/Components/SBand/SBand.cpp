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
    // Only process if radio is configured
    if (!m_configured) {
        return;
    }

    // Queue RX handler only if not already queued
    if (!m_rxHandlerQueued) {
        m_rxHandlerQueued = true;
        this->deferredRxHandler_internalInterfaceInvoke();
    }
}

void SBand ::deferredRxHandler_internalInterfaceHandler() {
    // Check IRQ status
    uint16_t irqStatus = this->m_rlb_radio.getIrqStatus();

    // Only process if RX_DONE
    if (irqStatus & RADIOLIB_SX128X_IRQ_RX_DONE) {
        // Process received data
        SX1280* radio = &this->m_rlb_radio;
        uint8_t data[256] = {0};
        size_t len = radio->getPacketLength();
        radio->readData(data, len);
        radio->startReceive(RADIOLIB_SX128X_RX_TIMEOUT_INF);

        Fw::Buffer buffer = this->allocate_out(0, static_cast<FwSizeType>(len));
        if (buffer.isValid()) {
            (void)::memcpy(buffer.getData(), data, len);
            ComCfg::FrameContext frameContext;
            this->dataOut_out(0, buffer, frameContext);
        }
    }

    // Clear the queued flag
    m_rxHandlerQueued = false;
}

void SBand ::deferredTxHandler_internalInterfaceHandler() {
    // Re-enable RX mode after transmission
    this->enableRx();
}

// ----------------------------------------------------------------------
// Handler implementations for Com interface
// ----------------------------------------------------------------------

void SBand ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // Only process if radio is configured
    if (!m_configured) {
        Fw::Success failureStatus = Fw::Success::FAILURE;
        this->dataReturnOut_out(0, data, context);
        this->comStatusOut_out(0, failureStatus);
        return;
    }

    // Switch to TX mode
    this->rxEnable_out(0, Fw::Logic::LOW);
    this->txEnable_out(0, Fw::Logic::HIGH);

    // Transmit data
    int16_t state = this->m_rlb_radio.transmit(data.getData(), data.getSize());
    FW_ASSERT(state == RADIOLIB_ERR_NONE);

    // Return buffer and status
    Fw::Success returnStatus = Fw::Success::SUCCESS;
    this->dataReturnOut_out(0, data, context);
    this->comStatusOut_out(0, returnStatus);

    // Queue deferred handler to re-enable RX
    this->deferredTxHandler_internalInterfaceInvoke();
}

void SBand ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // Deallocate the buffer
    this->deallocate_out(0, data);
}

void SBand ::enableRx() {
    this->txEnable_out(0, Fw::Logic::LOW);
    this->rxEnable_out(0, Fw::Logic::HIGH);

    SX1280* radio = &this->m_rlb_radio;

    int16_t state = radio->standby();
    FW_ASSERT(state == RADIOLIB_ERR_NONE);
    state = radio->startReceive(RADIOLIB_SX128X_RX_TIMEOUT_INF);
    FW_ASSERT(state == RADIOLIB_ERR_NONE);
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

void SBand ::configureRadio() {
    int16_t state = this->configure_radio();
    FW_ASSERT(state == RADIOLIB_ERR_NONE);

    // Mark as configured
    m_configured = true;

    Fw::Success status = Fw::Success::SUCCESS;
    this->comStatusOut_out(0, status);

    this->enableRx();
}

}  // namespace Components
