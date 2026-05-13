// ======================================================================
// \title  SBand.cpp
// \author jrpear
// \brief  cpp file for SBand component implementation class
// ======================================================================

#define RADIOLIB_LOW_LEVEL 1

#include "PROVESFlightControllerReference/Components/SBand/SBand.hpp"

#include <RadioLib.h>

#include <Fw/Buffer/Buffer.hpp>
#include <Fw/Logger/Logger.hpp>

#include "FprimeHal.hpp"

namespace Components {

static float bandwidthEnumToKHz(SBandBandwidth bw) {
    switch (bw.e) {
        case SBandBandwidth::BW_203_125_KHZ:
            return 203.125f;
        case SBandBandwidth::BW_406_25_KHZ:
            return 406.25f;
        case SBandBandwidth::BW_812_5_KHZ:
            return 812.5f;
        case SBandBandwidth::BW_1625_KHZ:
            return 1625.0f;
        default:
            FW_ASSERT(false);  // Should be unreachable if enum is valid
            return 0.0f;
    }
}

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

SBand ::SBand(const char* const compName)
    : SBandComponentBase(compName),
      m_rlb_hal(this),
      m_rlb_module(&m_rlb_hal, SBAND_PIN_CS, SBAND_PIN_IRQ, SBAND_PIN_RST),
      m_rlb_radio(&m_rlb_module) {}

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
    Fw::Buffer mutableData = data;  // hack to get around const-ness

    if (this->m_transmit_enabled != SBandTransmitState::ENABLED) {
        this->dataReturnOut_out(0, mutableData, context);
        this->comStatusOut_out(0, returnStatus);
        return;
    }

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

    this->dataReturnOut_out(0, mutableData, context);
    this->comStatusOut_out(0, returnStatus);

    status = this->enableRx();
}

// ----------------------------------------------------------------------
// Handler implementations for Com interface
// ----------------------------------------------------------------------

void SBand ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
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
    Fw::ParamValid isValid = Fw::ParamValid::INVALID;
    const SBandDataRate dataRate = this->paramGet_DATA_RATE(isValid);
    FW_ASSERT((isValid == Fw::ParamValid::VALID) || (isValid == Fw::ParamValid::DEFAULT),
              static_cast<FwAssertArgType>(isValid));
    const SBandCodingRate codingRate = this->paramGet_CODING_RATE(isValid);
    FW_ASSERT((isValid == Fw::ParamValid::VALID) || (isValid == Fw::ParamValid::DEFAULT),
              static_cast<FwAssertArgType>(isValid));
    const SBandBandwidth bandwidth = this->paramGet_BANDWIDTH_RX(isValid);
    FW_ASSERT((isValid == Fw::ParamValid::VALID) || (isValid == Fw::ParamValid::DEFAULT),
              static_cast<FwAssertArgType>(isValid));

    this->txEnable_out(0, Fw::Logic::LOW);
    this->rxEnable_out(0, Fw::Logic::HIGH);

    SX1280* radio = &this->m_rlb_radio;

    int16_t state = radio->standby();
    if (state != RADIOLIB_ERR_NONE) {
        this->log_WARNING_HI_RadioLibFailed(state);
        return Status::ERROR;
    }

    state = radio->setSpreadingFactor(static_cast<uint8_t>(dataRate.e));
    if (state != RADIOLIB_ERR_NONE) {
        this->log_WARNING_HI_RadioLibFailed(state);
        return Status::ERROR;
    }

    state = radio->setCodingRate(static_cast<uint8_t>(codingRate.e));
    if (state != RADIOLIB_ERR_NONE) {
        this->log_WARNING_HI_RadioLibFailed(state);
        return Status::ERROR;
    }

    state = radio->setBandwidth(bandwidthEnumToKHz(bandwidth));
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
    Fw::ParamValid isValid = Fw::ParamValid::INVALID;
    const SBandDataRate dataRate = this->paramGet_DATA_RATE(isValid);
    FW_ASSERT((isValid == Fw::ParamValid::VALID) || (isValid == Fw::ParamValid::DEFAULT),
              static_cast<FwAssertArgType>(isValid));
    const SBandCodingRate codingRate = this->paramGet_CODING_RATE(isValid);
    FW_ASSERT((isValid == Fw::ParamValid::VALID) || (isValid == Fw::ParamValid::DEFAULT),
              static_cast<FwAssertArgType>(isValid));
    const SBandBandwidth bandwidth = this->paramGet_BANDWIDTH_TX(isValid);
    FW_ASSERT((isValid == Fw::ParamValid::VALID) || (isValid == Fw::ParamValid::DEFAULT),
              static_cast<FwAssertArgType>(isValid));

    this->rxEnable_out(0, Fw::Logic::LOW);
    this->txEnable_out(0, Fw::Logic::HIGH);

    SX1280* radio = &this->m_rlb_radio;

    int16_t state = radio->standby();
    if (state != RADIOLIB_ERR_NONE) {
        this->log_WARNING_HI_RadioLibFailed(state);
        return Status::ERROR;
    }

    state = radio->setSpreadingFactor(static_cast<uint8_t>(dataRate.e));
    if (state != RADIOLIB_ERR_NONE) {
        this->log_WARNING_HI_RadioLibFailed(state);
        return Status::ERROR;
    }

    state = radio->setCodingRate(static_cast<uint8_t>(codingRate.e));
    if (state != RADIOLIB_ERR_NONE) {
        this->log_WARNING_HI_RadioLibFailed(state);
        return Status::ERROR;
    }

    state = radio->setBandwidth(bandwidthEnumToKHz(bandwidth));
    if (state != RADIOLIB_ERR_NONE) {
        this->log_WARNING_HI_RadioLibFailed(state);
        return Status::ERROR;
    }

    return Status::SUCCESS;
}

SBand::Status SBand ::configureRadio() {
    Fw::ParamValid isValid = Fw::ParamValid::INVALID;
    const SBandDataRate dataRate = this->paramGet_DATA_RATE(isValid);
    FW_ASSERT((isValid == Fw::ParamValid::VALID) || (isValid == Fw::ParamValid::DEFAULT),
              static_cast<FwAssertArgType>(isValid));
    const SBandCodingRate codingRate = this->paramGet_CODING_RATE(isValid);
    FW_ASSERT((isValid == Fw::ParamValid::VALID) || (isValid == Fw::ParamValid::DEFAULT),
              static_cast<FwAssertArgType>(isValid));
    const SBandBandwidth bandwidthRx = this->paramGet_BANDWIDTH_RX(isValid);
    FW_ASSERT((isValid == Fw::ParamValid::VALID) || (isValid == Fw::ParamValid::DEFAULT),
              static_cast<FwAssertArgType>(isValid));

    float frequencyMHz = 2400.0;
    float bandwidthKHz = bandwidthEnumToKHz(bandwidthRx);
    uint8_t spreadingFactor = static_cast<uint8_t>(dataRate.e);
    uint8_t codingRateValue = static_cast<uint8_t>(codingRate.e);
    uint8_t syncWord = RADIOLIB_SX128X_SYNC_WORD_PRIVATE;
    int8_t outputPowerDbm = 13;  // 13 dBm is max
    uint16_t preambleLength = 12;

    int16_t state = this->m_rlb_radio.begin(frequencyMHz, bandwidthKHz, spreadingFactor, codingRateValue, syncWord,
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

    Status rx_status = this->enableRx();
    if (rx_status != Status::SUCCESS) {
        return Status::ERROR;
    }

    m_configured = true;

    // Only start ping-pong protocol if transmit is enabled
    if (this->m_transmit_enabled == SBandTransmitState::ENABLED) {
        Fw::Success status = Fw::Success::SUCCESS;
        this->comStatusOut_out(0, status);
    }

    return Status::SUCCESS;
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void SBand ::TRANSMIT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, SBandTransmitState enabled) {
    // Invoke internal port to handle state change asynchronously
    // This prevents concurrent access issues with m_transmit_enabled
    this->deferredTransmitCmd_internalInterfaceInvoke(enabled);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void SBand ::deferredTransmitCmd_internalInterfaceHandler(const SBandTransmitState& enabled) {
    if (enabled == SBandTransmitState::ENABLED) {
        // Start the ping-pong protocol if we are disabled
        if (this->m_transmit_enabled == SBandTransmitState::DISABLED) {
            // Must transition to ENABLED **BEFORE** calling comStatusOut
            this->m_transmit_enabled = SBandTransmitState::ENABLED;
            Fw::Success comStatus = Fw::Success::SUCCESS;
            this->comStatusOut_out(0, comStatus);
        }
    } else {
        this->m_transmit_enabled = SBandTransmitState::DISABLED;
    }
}

}  // namespace Components
