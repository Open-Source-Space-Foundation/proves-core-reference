// ======================================================================
// \title  MyComponent.hpp
// \author jrpear
// \brief  hpp file for MyComponent component implementation class
// ======================================================================

#ifndef Components_MyComponent_HPP
#define Components_MyComponent_HPP

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

    //! Handler implementation for command FOO
    //!
    //! Command for testing
    void FOO_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                        U32 cmdSeq            //!< The command sequence number
                        ) override;

    //! Handler implementation for command RESET
    //!
    //! Reset Radio Module
    void RESET_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                          U32 cmdSeq            //!< The command sequence number
                          ) override;

    //! SX1280 SPI Commands

    void spiSetModulationParams();
    void spiSetTxParams();
    void spiSetTxContinuousPreamble();
    void spiSetTxContinuousWave();
    void spiSetRfFrequency();
    void spiSetTx();
    void spiSetStandby();
    U8 spiGetStatus();
};

}  // namespace Components

#endif
