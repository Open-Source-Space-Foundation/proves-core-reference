// ======================================================================
// \title  MyComponent.hpp
// \author jrpear
// \brief  hpp file for MyComponent component implementation class
// ======================================================================

#ifndef Components_MyComponent_HPP
#define Components_MyComponent_HPP

#include "FprimeHal.hpp"
#include "FprimeZephyrReference/Components/MyComponent/MyComponentComponentAc.hpp"

namespace Components {

class MyComponent final : public MyComponentComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct MyComponent object
    MyComponent(const char* const compName  //!< The component name
    );

    //! Destroy MyComponent object
    ~MyComponent();

    using MyComponentComponentBase::getIRQLine_out;
    using MyComponentComponentBase::getTime;
    using MyComponentComponentBase::spiSend_out;

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

    //! Handler implementation for command READ_DATA
    //!
    //! Command to read recv data buffer
    void READ_DATA_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                              U32 cmdSeq            //!< The command sequence number
                              ) override;

    //! Handler implementation for command RESET
    //!
    //! Reset Radio Module
    void RESET_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                          U32 cmdSeq            //!< The command sequence number
                          ) override;

    // Configure the SX1280 radio (setup and parameter tuning)
    int16_t configure_radio();

  private:
    FprimeHal m_rlb_hal;  //!< RadioLib HAL instance
    Module m_rlb_module;  //!< RadioLib Module instance
    SX1280 m_rlb_radio;   //!< RadioLib SX1280 radio instance
};

}  // namespace Components

#endif
