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
    : SBandComponentBase(compName), m_rlb_hal(this), m_rlb_module(&m_rlb_hal, 0, 5, 6), m_rlb_radio(&m_rlb_module) {}

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
        int16_t state = radio->readData(data, len);

        if (state != RADIOLIB_ERR_NONE) {
            this->log_WARNING_HI_RadioLibFailed(state);
        } else {
            Fw::Buffer buffer = this->allocate_out(0, static_cast<FwSizeType>(len));
            if (buffer.isValid()) {
                (void)::memcpy(buffer.getData(), data, len);
                ComCfg::FrameContext frameContext;
                this->dataOut_out(0, buffer, frameContext);

                // Log RSSI and SNR for received packet
                float rssi = radio->getRSSI();
                float snr = radio->getSNR();
                this->tlmWrite_LastRssi(rssi);
                this->tlmWrite_LastSnr(snr);

                // Clear throttled warnings on success
                this->log_WARNING_HI_RadioLibFailed_ThrottleClear();
                this->log_WARNING_HI_AllocationFailed_ThrottleClear();
            } else {
                this->log_WARNING_HI_AllocationFailed(static_cast<FwSizeType>(len));
            }
        }

        // Re-enable receive mode
        state = radio->startReceive(RADIOLIB_SX128X_RX_TIMEOUT_INF);
        if (state != RADIOLIB_ERR_NONE) {
            this->log_WARNING_HI_RadioLibFailed(state);
        }
    }

    // Clear the queued flag
    m_rxHandlerQueued = false;
}

void SBand ::deferredTxHandler_internalInterfaceHandler(const Fw::Buffer& data, const ComCfg::FrameContext& context) {
    Fw::Success returnStatus = Fw::Success::FAILURE;

    // Enable transmit mode
    Status status = this->enableTx();
    if (status == Status::SUCCESS) {
        // Transmit data
        int16_t state = this->m_rlb_radio.transmit(data.getData(), data.getSize());
        if (state != RADIOLIB_ERR_NONE) {
            this->log_WARNING_HI_RadioLibFailed(state);
            returnStatus = Fw::Success::FAILURE;
        } else {
            returnStatus = Fw::Success::SUCCESS;
            // Clear throttled warnings on success
            this->log_WARNING_HI_RadioLibFailed_ThrottleClear();
        }
    }

    // Return buffer and status (need mutable copy for dataReturnOut_out)
    Fw::Buffer mutableData = data;
    this->dataReturnOut_out(0, mutableData, context);
    this->comStatusOut_out(0, returnStatus);

    // Re-enable RX mode after transmission
    status = this->enableRx();
    // enableRx will log RadioLibFailed internally if it fails
}

// ----------------------------------------------------------------------
// Handler implementations for Com interface
// ----------------------------------------------------------------------

void SBand ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // Only process if radio is configured
    if (!m_configured) {
        this->log_WARNING_HI_RadioNotConfigured();
        Fw::Success failureStatus = Fw::Success::FAILURE;
        this->dataReturnOut_out(0, data, context);
        this->comStatusOut_out(0, failureStatus);
        return;
    }

    // Queue deferred handler to perform transmission
    this->deferredTxHandler_internalInterfaceInvoke(data, context);
}

void SBand ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // Deallocate the buffer
    this->deallocate_out(0, data);
}

SBand::Status SBand ::enableRx() {
    this->txEnable_out(0, Fw::Logic::LOW);
    this->rxEnable_out(0, Fw::Logic::HIGH);

    SX1280* radio = &this->m_rlb_radio;

    int16_t state = radio->standby();
    if (state != RADIOLIB_ERR_NONE) {
        this->log_WARNING_HI_RadioLibFailed(state);
        return Status::ERROR;
    }

    state = radio->startReceive(RADIOLIB_SX128X_RX_TIMEOUT_INF);
    if (state != RADIOLIB_ERR_NONE) {
        this->log_WARNING_HI_RadioLibFailed(state);
        return Status::ERROR;
    }
    return Status::SUCCESS;
}

SBand::Status SBand ::enableTx() {
    this->rxEnable_out(0, Fw::Logic::LOW);
    this->txEnable_out(0, Fw::Logic::HIGH);

    SX1280* radio = &this->m_rlb_radio;

    int16_t state = radio->standby();
    if (state != RADIOLIB_ERR_NONE) {
        this->log_WARNING_HI_RadioLibFailed(state);
        return Status::ERROR;
    }
    return Status::SUCCESS;
}

SBand::Status SBand ::configure_radio() {
    float frequencyMHz = 2400.0;
    float bandwidthKHz = 406.25;
    uint8_t spreadingFactor = 7;
    uint8_t codingRate = 5;
    uint8_t syncWord = RADIOLIB_SX128X_SYNC_WORD_PRIVATE;
    int8_t outputPowerDbm = 13;  // 13 dBm is max
    uint16_t preambleLength = 12;

    int16_t state = this->m_rlb_radio.begin(frequencyMHz, bandwidthKHz, spreadingFactor, codingRate, syncWord,
                                            outputPowerDbm, preambleLength);
    if (state != RADIOLIB_ERR_NONE) {
        this->log_WARNING_HI_RadioLibFailed(state);
        return Status::ERROR;
    }

    state = this->m_rlb_radio.setPacketParamsLoRa(preambleLength, RADIOLIB_SX128X_LORA_HEADER_EXPLICIT, 255,
                                                  RADIOLIB_SX128X_LORA_CRC_ON, RADIOLIB_SX128X_LORA_IQ_STANDARD);
    if (state != RADIOLIB_ERR_NONE) {
        this->log_WARNING_HI_RadioLibFailed(state);
        return Status::ERROR;
    }

    return Status::SUCCESS;
}

SBand::Status SBand ::configureRadio() {
    Status config_status = this->configure_radio();
    if (config_status != Status::SUCCESS) {
        // configure_radio logs RadioLibFailed internally
        return Status::ERROR;
    }

    // Mark as configured
    m_configured = true;

    // Enable RX mode
    Status rx_status = this->enableRx();
    if (rx_status != Status::SUCCESS) {
        // enableRx logs RadioLibFailed internally
        return Status::ERROR;
    }

    // Send success status
    Fw::Success status = Fw::Success::SUCCESS;
    this->comStatusOut_out(0, status);

    return Status::SUCCESS;
}

}  // namespace Components
