// ======================================================================
// \title  Burnwire.hpp
// \author aldjia
// \brief  hpp file for Burnwire component implementation class
// ======================================================================

#ifndef Components_Burnwire_HPP
#define Components_Burnwire_HPP

#include "FprimeZephyrReference/Components/Burnwire/BurnwireComponentAc.hpp"

namespace Components {

class Burnwire final : public BurnwireComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Burnwire object
    Burnwire(const char* const compName  //!< The component name
    );

    //! Destroy Burnwire object
    ~Burnwire();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for stop
    //!
    //! Port to start and stop the burnwire
    void stop_handler(FwIndexType portNum  //!< The port number
                      ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command START_BURNWIRE
    void START_BURNWIRE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    //! Handler implementation for command STOP_BURNWIRE
    void STOP_BURNWIRE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    Fw::On m_state = Fw::On::OFF;  // keeps track if burnwire is on or off
};

}  // namespace Components

#endif
