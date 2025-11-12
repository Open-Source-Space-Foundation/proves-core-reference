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
    enum Status {
        SUCCESS,
        FAILURE,
    };
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct StartupManager object
    StartupManager(const char* const compName  //!< The component name
    );

    //! Destroy StartupManager object
    ~StartupManager();

    //! \brief read and increment the boot count
    //!
    //! Reads the boot count from the boot count file, increments it, and writes it back to the file. If the read
    //! fails, the boot count will be initialized to 1. If the write fails, a warning will be emitted.
    //!
    //! \warning this function will modify the boot count file on disk.
    //!
    //! \return The updated boot count
    FwSizeType update_boot_count();

    //! \brief get and possibly initialize the quiescence start time
    //!
    //! Reads the quiescence start time from the quiescence start time file. If the read fails, the current time is
    //! written to the file and returned.
    //!
    //! \warning this function will modify the quiescence start time file on disk if it does not already exist.
    //!
    //! \return The quiescence start time
    Fw::Time update_quiescence_start();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for completeSequence
    void completeSequence_handler(FwIndexType portNum,             //!< The port number
                                  FwOpcodeType opCode,             //!< Command Op Code
                                  U32 cmdSeq,                      //!< Command Sequence
                                  const Fw::CmdResponse& response  //!< The command response argument
                                  ) override;

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
