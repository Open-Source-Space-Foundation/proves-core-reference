// ======================================================================
// \title  Watchdog.hpp
// \author moisesmata
// \brief  hpp file for Watchdog component implementation class
// ======================================================================

#ifndef Components_Watchdog_HPP
#define Components_Watchdog_HPP

#include "FprimeZephyrReference/Components/Watchdog/WatchdogComponentAc.hpp"
// Need to explicitly include because Fw.On is no longer a tlm channel I believe
#include <atomic>
#include "Fw/Types/OnEnumAc.hpp"

namespace Components {

class Watchdog : public WatchdogComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Watchdog object
    Watchdog(const char* const compName  //!< The component name
    );

    //! Destroy Watchdog object
    ~Watchdog();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for user-defined typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    //!
    //! Port receiving calls from the rate group
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    //! Handler implementation for start
    //!
    //! Port to start the watchdog
    void start_handler(FwIndexType portNum  //!< The port number
                       ) override;

    //! Handler implementation for stop
    //!
    //! Port to stop the watchdog
    void stop_handler(FwIndexType portNum  //!< The port number
                      ) override;

    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command START_WATCHDOG
    //!
    //! Command to start the watchdog
    void START_WATCHDOG_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                   U32 cmdSeq            //!< The command sequence number
                                   ) override;

    //! Handler implementation for command STOP_WATCHDOG
    //!
    //! Command to stop the watchdog
    void STOP_WATCHDOG_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                  U32 cmdSeq            //!< The command sequence number
                                  ) override;

    std::atomic_bool m_run{true};  //! Flag to start or stop the watchdog
    Fw::On m_state = Fw::On::OFF;  //! Keeps track of GPIO state
    U32 m_transitions = 0;         //! The number of on/off transitions that have occurred
                                   //! from FSW boot up
};

}  // namespace Components

#endif
