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
#include <Fw/Time/Time.hpp>
#include <Os/Task.hpp>

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
    : SBandComponentBase(compName), m_rlb_radio(this), m_radio(&m_rlb_radio), m_faultPolicy() {}

SBand ::~SBand() {}

void SBand ::setRadioIf(SBandRadioIf* radio) {
    m_radio = (radio != nullptr) ? radio : &m_rlb_radio;
}

// ----------------------------------------------------------------------
// Fault management (S-BAND-REINTEGRATION-PLAN.md decision D3)
// ----------------------------------------------------------------------

bool SBand ::isFaulted() const {
    return this->m_faultPolicy.decision() == SBandFaultPolicy::Decision::FAULTED;
}

SBand::Status SBand ::handleRadioResult(int16_t state) {
    if (state == RADIOLIB_ERR_NONE) {
        this->m_faultPolicy.operationSucceeded();
        this->log_WARNING_HI_RadioLibFailed_ThrottleClear();
        this->tlmWrite_ConsecutiveRadioFailures(0);
        return Status::SUCCESS;
    }

    this->log_WARNING_HI_RadioLibFailed(state);
    this->m_faultPolicy.operationFailed();
    this->tlmWrite_ConsecutiveRadioFailures(this->m_faultPolicy.consecutiveFailures());
    return Status::ERROR;
}

void SBand ::performHardwareReset() {
    // Toggle the SX1280's nRST line directly: nRST is a component-level GPIO
    // output port (resetSend_out / FprimeHal's SBAND_PIN_RST digitalWrite),
    // not part of the SBandRadioIf seam, which only covers SX1280 driver
    // calls. Pulse width/settle time are placeholders pending PR 3 HWIL
    // bench tuning.
    this->resetSend_out(0, Fw::Logic::LOW);
    Os::Task::delay(Fw::TimeInterval(0, 1000));  // 1 ms nRST pulse
    this->resetSend_out(0, Fw::Logic::HIGH);
    Os::Task::delay(Fw::TimeInterval(0, 1000));  // allow the SX1280 to boot before further SPI
}

void SBand ::applyFaultDecision() {
    // Bounded: each REQUEST_RESET branch below calls resetCompleted(), and
    // SBandFaultPolicy::RESET_ATTEMPT_LIMIT resets without an intervening
    // success latches FAULTED (terminating the loop). The iteration cap is
    // a defensive backstop only -- see SBandFaultPolicy.hpp.
    for (std::uint32_t guard = 0; guard <= SBandFaultPolicy::RESET_ATTEMPT_LIMIT; guard++) {
        SBandFaultPolicy::Decision decision = this->m_faultPolicy.decision();

        if (decision == SBandFaultPolicy::Decision::NONE) {
            return;
        }

        if (decision == SBandFaultPolicy::Decision::FAULTED) {
            this->tlmWrite_RadioFaulted(true);
            if (!this->m_faultEvrEmitted) {
                this->log_WARNING_HI_RadioFaultLatched(this->m_faultPolicy.resetsSinceSuccess());
                this->m_faultEvrEmitted = true;
            }
            return;
        }

        // REQUEST_RESET: perform the nRST reset, then attempt a fresh
        // re-init. The re-init's own radio calls flow back through
        // handleRadioResult(), which may push the decision to
        // REQUEST_RESET or FAULTED again -- re-checked at the top of the
        // next iteration.
        this->log_WARNING_LO_RadioResetRequested(this->m_faultPolicy.consecutiveFailures());
        this->performHardwareReset();
        this->m_faultPolicy.resetCompleted();
        this->tlmWrite_ConsecutiveRadioFailures(0);
        this->tlmWrite_RadioResetCount(this->m_faultPolicy.resetsSinceSuccess());

        if (this->m_faultPolicy.decision() != SBandFaultPolicy::Decision::FAULTED) {
            (void)this->configureRadio();
        }
    }
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void SBand ::run_handler(FwIndexType portNum, U32 context) {
    // Only process if radio is configured and not latched FAULTED (D3: a
    // FAULTED radio makes run ticks no-ops -- no further radio-interface
    // calls are made until a ground RESET_RADIO succeeds).
    if (!m_configured || this->isFaulted()) {
        return;
    }

    // Queue RX handler only if not already queued. m_rxHandlerQueued is
    // exchanged atomically (ported from s-band-speedup 9b240d0): run_handler
    // executes on the rate-group thread while deferredRxHandler runs on
    // SBand's own thread, so a plain read-then-write here would race.
    if (!m_rxHandlerQueued.exchange(true)) {
        this->deferredRxHandler_internalInterfaceInvoke();
    }
}

void SBand ::deferredRxHandler_internalInterfaceHandler() {
    if (this->isFaulted()) {
        m_rxHandlerQueued = false;
        return;
    }

    // Check IRQ status
    uint16_t irqStatus = m_radio->getIrqStatus();

    // Only process if RX_DONE
    if (irqStatus & RADIOLIB_SX128X_IRQ_RX_DONE) {
        bool haveData = false;
        Fw::Buffer buffer;

        // Allocate directly from the packet length, then read RadioLib data
        // straight into the allocated Fw::Buffer -- no intermediate stack
        // array (the 256-byte array this replaced was implicated in the
        // #299 SBand-thread stack overflow).
        size_t len = m_radio->getPacketLength();
        buffer = this->allocate_out(0, static_cast<FwSizeType>(len));

        if (!buffer.isValid()) {
            this->log_WARNING_HI_AllocationFailed(static_cast<FwSizeType>(len));
        } else {
            int16_t state = m_radio->readData(buffer.getData(), len);
            if (this->handleRadioResult(state) == Status::SUCCESS) {
                this->log_WARNING_HI_AllocationFailed_ThrottleClear();

                // Log RSSI and SNR for received packet
                this->tlmWrite_LastRssi(m_radio->getRSSI());
                this->tlmWrite_LastSnr(m_radio->getSNR());
                haveData = true;
            } else {
                this->deallocate_out(0, buffer);
            }
        }

        // Re-enable receive mode. Ported from s-band-speedup 17df3ec: go
        // through enableRx() (not a raw startReceive()) so RF params are
        // reapplied on every re-arm, not just at configureRadio() time.
        this->enableRx();

        // Ported from s-band-speedup 8aeee2f: send the frame out only after
        // every SPI operation for this packet is done (readData, RSSI/SNR,
        // enableRx) -- RadioLib and the flash filesystem both use the SPI
        // bus, and dataOut_out's synchronous downstream call chain held the
        // bus contended with flash when it ran before those SPI ops.
        if (haveData) {
            ComCfg::FrameContext frameContext;
            this->dataOut_out(0, buffer, frameContext);
        }
    }

    // Clear the queued flag
    m_rxHandlerQueued = false;

    this->applyFaultDecision();
}

void SBand ::deferredTxHandler_internalInterfaceHandler(const Fw::Buffer& data, const ComCfg::FrameContext& context) {
    Fw::Success returnStatus = Fw::Success::FAILURE;
    Fw::Buffer mutableData = data;  // hack to get around const-ness

    if ((this->m_transmit_enabled != SBandTransmitState::ENABLED) || this->isFaulted()) {
        this->dataReturnOut_out(0, mutableData, context);
        this->comStatusOut_out(0, returnStatus);
        return;
    }

    // Enable transmit mode
    int16_t state = this->enableTx();
    if (state == RADIOLIB_ERR_NONE) {
        // Transmit data
        state = m_radio->transmit(data.getData(), data.getSize());
        if (this->handleRadioResult(state) == Status::SUCCESS) {
            returnStatus = Fw::Success::SUCCESS;
        }
    }

    this->dataReturnOut_out(0, mutableData, context);
    this->comStatusOut_out(0, returnStatus);

    if (!this->isFaulted()) {
        this->enableRx();
    }

    this->applyFaultDecision();
}

// ----------------------------------------------------------------------
// Handler implementations for Com interface
// ----------------------------------------------------------------------

void SBand ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // FAULTED and !configured both degrade to "no S-Band": the buffer is
    // returned and comStatus is still emitted immediately so the com queue
    // never starves, but the radio itself is never touched.
    if (!m_configured || this->isFaulted()) {
        if (!m_configured) {
            this->log_WARNING_HI_RadioNotConfigured();
        }
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

int16_t SBand ::enableRx() {
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

    int16_t state = m_radio->standby();
    if (this->handleRadioResult(state) != Status::SUCCESS) {
        return state;
    }

    state = m_radio->setSpreadingFactor(static_cast<uint8_t>(dataRate.e));
    if (this->handleRadioResult(state) != Status::SUCCESS) {
        return state;
    }

    state = m_radio->setCodingRate(static_cast<uint8_t>(codingRate.e));
    if (this->handleRadioResult(state) != Status::SUCCESS) {
        return state;
    }

    state = m_radio->setBandwidth(bandwidthEnumToKHz(bandwidth));
    if (this->handleRadioResult(state) != Status::SUCCESS) {
        return state;
    }

    state = m_radio->startReceive();
    (void)this->handleRadioResult(state);
    return state;
}

int16_t SBand ::enableTx() {
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

    int16_t state = m_radio->standby();
    if (this->handleRadioResult(state) != Status::SUCCESS) {
        return state;
    }

    state = m_radio->setSpreadingFactor(static_cast<uint8_t>(dataRate.e));
    if (this->handleRadioResult(state) != Status::SUCCESS) {
        return state;
    }

    state = m_radio->setCodingRate(static_cast<uint8_t>(codingRate.e));
    if (this->handleRadioResult(state) != Status::SUCCESS) {
        return state;
    }

    state = m_radio->setBandwidth(bandwidthEnumToKHz(bandwidth));
    (void)this->handleRadioResult(state);
    return state;
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

    int16_t state = m_radio->begin(frequencyMHz, bandwidthKHz, spreadingFactor, codingRateValue, syncWord,
                                   outputPowerDbm, preambleLength);
    if (this->handleRadioResult(state) != Status::SUCCESS) {
        return Status::ERROR;
    }

    state = m_radio->setPacketParamsLoRa(preambleLength, RADIOLIB_SX128X_LORA_HEADER_EXPLICIT, 255,
                                         RADIOLIB_SX128X_LORA_CRC_ON, RADIOLIB_SX128X_LORA_IQ_STANDARD);
    if (this->handleRadioResult(state) != Status::SUCCESS) {
        return Status::ERROR;
    }

    int16_t rx_state = this->enableRx();
    if (rx_state != RADIOLIB_ERR_NONE) {
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

void SBand ::RESET_RADIO_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    bool wasFaulted = this->isFaulted();

    // Re-arm the fault policy (clears a FAULTED latch, if any) and attempt
    // a full re-init.
    this->m_faultPolicy.groundResetRequested();
    this->m_faultEvrEmitted = false;
    this->tlmWrite_RadioFaulted(false);
    this->tlmWrite_ConsecutiveRadioFailures(0);
    this->tlmWrite_RadioResetCount(0);

    Status status = this->configureRadio();

    // The re-init attempt above flows through handleRadioResult(); apply
    // whatever decision that produced (it may immediately re-escalate to
    // REQUEST_RESET/FAULTED if the underlying hardware fault persists).
    this->applyFaultDecision();

    if (wasFaulted && !this->isFaulted()) {
        this->log_ACTIVITY_HI_RadioFaultCleared();
    }

    Fw::CmdResponse response =
        ((status == Status::SUCCESS) && !this->isFaulted()) ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR;
    this->cmdResponse_out(opCode, cmdSeq, response);
}

}  // namespace Components
