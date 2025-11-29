// ======================================================================
// \title  LoRa2.hpp
// \author jrpear
// \brief  hpp file for LoRa2 component implementation class
// ======================================================================

#ifndef Components_LoRa2_HPP
#define Components_LoRa2_HPP

#include "FprimeHal.hpp"
#include "FprimeZephyrReference/Components/LoRa2/LoRa2ComponentAc.hpp"

namespace Components {

class LoRa2 final : public LoRa2ComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct LoRa2 object
    LoRa2(const char* const compName  //!< The component name
    );

    //! Destroy LoRa2 object
    ~LoRa2();

    using LoRa2ComponentBase::getIRQLine_out;
    using LoRa2ComponentBase::getTime;
    using LoRa2ComponentBase::spiSend_out;

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

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command TRANSMIT
    //!
    //! Command to transmit data
    void TRANSMIT_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                             U32 cmdSeq            //!< The command sequence number
                             ) override;

    //! Handler implementation for command RECEIVE
    //!
    //! Command to begin receive
    void RECEIVE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                            U32 cmdSeq            //!< The command sequence number
                            ) override;

    // Configure the SX1280 radio (setup and parameter tuning)
    int16_t configure_radio();

  private:
    FprimeHal m_rlb_hal;  //!< RadioLib HAL instance
    Module m_rlb_module;  //!< RadioLib Module instance
    SX1280 m_rlb_radio;   //!< RadioLib SX1280 radio instance
    bool wait_for_rx_fin = false;
};

}  // namespace Components

#endif
