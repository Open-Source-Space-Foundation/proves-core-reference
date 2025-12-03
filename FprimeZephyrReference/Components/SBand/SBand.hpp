// ======================================================================
// \title  SBand.hpp
// \author jrpear
// \brief  hpp file for SBand component implementation class
// ======================================================================

#ifndef Components_SBand_HPP
#define Components_SBand_HPP

#include "FprimeHal.hpp"
#include "FprimeZephyrReference/Components/SBand/SBandComponentAc.hpp"
#include "Os/Mutex.hpp"

namespace Components {

class SBand final : public SBandComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct SBand object
    SBand(const char* const compName  //!< The component name
    );

    //! Destroy SBand object
    ~SBand();

    void start();

    using SBandComponentBase::getIRQLine_out;
    using SBandComponentBase::getTime;
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

  private:
    // Configure the SX1280 radio (setup and parameter tuning)
    int16_t configure_radio();
    void enableRx();

  private:
    FprimeHal m_rlb_hal;  //!< RadioLib HAL instance
    Module m_rlb_module;  //!< RadioLib Module instance
    SX1280 m_rlb_radio;   //!< RadioLib SX1280 radio instance
    bool rx_mode = false;
    Os::Mutex m_mutex;  //!< Mutex for thread safety
};

}  // namespace Components

#endif
