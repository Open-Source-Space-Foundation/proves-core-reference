// ======================================================================
// \title  SBand.hpp
// \author jrpear
// \brief  hpp file for SBand component implementation class
// ======================================================================

#ifndef Components_SBand_HPP
#define Components_SBand_HPP

#include <atomic>

#include "PROVESFlightControllerReference/Components/SBand/RadioLibSBandRadio.hpp"
#include "PROVESFlightControllerReference/Components/SBand/SBandComponentAc.hpp"
#include "PROVESFlightControllerReference/Components/SBand/SBandFaultPolicy.hpp"
#include "PROVESFlightControllerReference/Components/SBand/SBandRadioIf.hpp"

namespace Components {

class SBand final : public SBandComponentBase {
  public:
    // Status returned from various SBand operations
    enum Status { ERROR, SUCCESS };

    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct SBand object
    SBand(const char* const compName  //!< The component name
    );

    //! Destroy SBand object
    ~SBand();

    //! Configure the radio and start operation
    Status configureRadio();

    //! Test seam: point the component at a different SBandRadioIf (e.g. a
    //! fake). Not used in flight; production wiring always uses the
    //! RadioLibSBandRadio owned by this object (m_rlb_radio).
    void setRadioIf(SBandRadioIf* radio);

    using SBandComponentBase::getBusyLine_out;
    using SBandComponentBase::getIRQLine_out;
    using SBandComponentBase::getTime;
    using SBandComponentBase::resetSend_out;
    using SBandComponentBase::spiSend_out;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    //!
    //! Port receiving calls from the rate group
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    //! Handler implementation for dataIn
    //!
    //! Data to be sent on the wire (coming in to the component)
    void dataIn_handler(FwIndexType portNum,  //!< The port number
                        Fw::Buffer& data,
                        const ComCfg::FrameContext& context) override;

    //! Handler implementation for dataReturnIn
    //!
    //! Port receiving back ownership of buffer sent out on dataOut
    void dataReturnIn_handler(FwIndexType portNum,  //!< The port number
                              Fw::Buffer& data,
                              const ComCfg::FrameContext& context) override;

    //! Handler implementation for deferredRxHandler
    //!
    //! Internal async handler for processing received data
    void deferredRxHandler_internalInterfaceHandler() override;

    //! Handler implementation for deferredTxHandler
    //!
    //! Internal async handler for processing transmitted data
    void deferredTxHandler_internalInterfaceHandler(const Fw::Buffer& data,
                                                    const ComCfg::FrameContext& context) override;

    //! Handler implementation for deferredTransmitCmd
    //!
    //! Internal async handler for processing TRANSMIT command state changes
    void deferredTransmitCmd_internalInterfaceHandler(const SBandTransmitState& enabled) override;

    //! Handler implementation for command TRANSMIT
    //!
    //! Start/stop transmission on the S-Band module
    void TRANSMIT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, SBandTransmitState enabled) override;

    //! Handler implementation for command RESET_RADIO
    //!
    //! Ground-commanded radio reset: groundResetRequested() + full re-init attempt
    void RESET_RADIO_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

  private:
    //! Enable receive mode
    //! \return RadioLib state (RADIOLIB_ERR_NONE on success)
    int16_t enableRx();

    //! Enable transmit mode
    //! \return RadioLib state (RADIOLIB_ERR_NONE on success)
    int16_t enableTx();

    //! True while the fault policy is latched FAULTED: no further
    //! radio-interface calls are made until a ground RESET_RADIO succeeds.
    bool isFaulted() const;

    //! Feed one radio-call outcome into the fault policy, update the
    //! associated telemetry, and emit RadioLibFailed on failure. Does not
    //! itself act on the resulting decision -- callers follow up with
    //! applyFaultDecision().
    Status handleRadioResult(int16_t state);

    //! React to the fault policy's current decision: perform a nRST reset
    //! and re-init attempt on REQUEST_RESET, latch telemetry/EVR on
    //! FAULTED. No-op when the decision is NONE.
    void applyFaultDecision();

    //! Toggle the radio's nRST line (the existing reset path already wired
    //! through resetSend_out/FprimeHal's SBAND_PIN_RST digitalWrite).
    void performHardwareReset();

  private:
    RadioLibSBandRadio m_rlb_radio;  //!< Production RadioLib-backed radio implementation
    SBandRadioIf* m_radio;           //!< Interface used by all handlers; defaults to &m_rlb_radio
    SBandFaultPolicy m_faultPolicy;  //!< Fault state machine (decision D3); see SBandFaultPolicy.hpp
    bool m_configured = false;       //!< Flag indicating radio is configured
    bool m_faultEvrEmitted = false;  //!< RadioFaultLatched is emitted once per FAULTED latch, not every tick
    std::atomic<bool> m_rxHandlerQueued{false};                            //!< Flag indicating RX handler is queued
    SBandTransmitState m_transmit_enabled = SBandTransmitState::DISABLED;  //!< Transmit state
};

}  // namespace Components

#endif
