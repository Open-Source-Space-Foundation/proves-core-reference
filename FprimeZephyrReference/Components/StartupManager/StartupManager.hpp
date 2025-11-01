// ======================================================================
// \title  StartupManager.hpp
// \author starchmd
// \brief  hpp file for StartupManager component implementation class
// ======================================================================

#ifndef Components_StartupManager_HPP
#define Components_StartupManager_HPP

#include <atomic>
#include "FprimeZephyrReference/Components/StartupManager/StartupManagerComponentAc.hpp"

namespace Components {

class StartupManager final : public StartupManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct StartupManager object
    StartupManager(const char* const compName  //!< The component name
    );

    //! Destroy StartupManager object
    ~StartupManager();

    // Update and return the boot count
    FwSizeType update_boot_count();

    // Get the quiescence start time
    Fw::Time get_quiescence_start();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    //!
    //! Check RTC time diff
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command WAIT_FOR_QUIESCENCE
    //!
    //! Command to wait for system quiescence before proceeding with start-up
    void WAIT_FOR_QUIESCENCE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                        U32 cmdSeq            //!< The command sequence number
                                        ) override;

  private:
    Fw::Time m_quiescence_start;   //!< Time of the start of the quiescence wait
    FwOpcodeType m_stored_opcode;  //!< Stored opcode for delayed response
    FwSizeType m_boot_count;       //!< Current boot count
    U32 m_stored_sequence;         //!< Stored sequence number for delayed response
    std::atomic<bool> m_waiting;   //!< Indicates if waiting for quiescence
};

}  // namespace Components

#endif
