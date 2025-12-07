// ======================================================================
// \title  SBand.hpp
// \author jrpear
// \brief  hpp file for SBand component implementation class
// ======================================================================

#ifndef Components_SBand_HPP
#define Components_SBand_HPP

#include "FprimeZephyrReference/Components/SBand/SBandComponentAc.hpp"

// Forward declare Zephyr device structure
struct device;

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

  private:
    // Configure the SX1280 radio (setup and parameter tuning)
    Status configure_radio();

    //! Enable receive mode
    Status enableRx();

    //! Enable transmit mode
    Status enableTx();

  private:
    const struct device* m_device;   //!< Zephyr device pointer
    bool m_configured = false;       //!< Flag indicating radio is configured
};

}  // namespace Components

#endif
