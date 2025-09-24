// ======================================================================
// \title  Burnwire.hpp
// \author aldjia
// \brief  hpp file for Burnwire component implementation class
// ======================================================================

#ifndef Components_Burnwire_HPP
#define Components_Burnwire_HPP

#include <atomic>
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

    //! Handler implementation for burnStart
    //!
    //! Port getting start signal
    void burnStart_handler(FwIndexType portNum  //!< The port number
                           ) override;

    //! Handler implementation for burnStop
    //!
    //! Port getting stop signal
    void burnStop_handler(FwIndexType portNum  //!< The port number
                          ) override;

    void schedIn_handler(FwIndexType portNum,  //!< The port number
                         U32 context           //!< The call order
                         ) override;

    void parameterUpdated(FwPrmIdType id) override;

    // helper functions
    void startBurn();

    void stopBurn();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command START_BURNWIRE
    void START_BURNWIRE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 max_duration) override;

    //! Handler implementation for command STOP_BURNWIRE
    void STOP_BURNWIRE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    Fw::On m_state = Fw::On::OFF;      // keeps track if burnwire is on or off
    std::atomic<U32> m_safetyCounter;  // makes this an atomic variable (so its set only in one command),
                                       // you read and write half the value bc a corrupted read could be dangerouts
    U32 m_safetyMaxCount = 5;          // max count for safety timer (default 10 seconds)
};

}  // namespace Components

#endif
