// ======================================================================
// \title  StartupManager.hpp
// \author starchmd
// \brief  hpp file for StartupManager component implementation class
// ======================================================================

#ifndef Components_StartupManager_HPP
#define Components_StartupManager_HPP

#include <atomic>

#include "PROVESFlightControllerReference/Components/StartupManager/StartupManagerComponentAc.hpp"

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

    //! \brief read and optionally increment the boot count
    //!
    //! Reads the boot count from the boot count file. If the read fails or the stored value is implausible
    //! (torn/corrupt write), the count is treated as unread and initializes to 1 on increment. Only the
    //! increment path writes the file; plain reads leave it untouched. If the write fails, a warning is
    //! emitted and the write is retried on subsequent run ticks.
    //!
    //! \warning this function will modify the boot count file on disk when increment is true.
    //!
    //! \return The updated boot count
    FwSizeType get_boot_count(bool increment);

    //! \brief durably persist the boot count via write-to-temp + atomic rename
    //!
    //! littlefs renames are atomic, so a reset landing mid-update leaves either the old or the new
    //! file — never a torn one.
    //!
    //! \return Status of the persist operation
    Status persist_boot_count(const Fw::StringBase& file_path, FwSizeType value);

    //! \brief get and possibly initialize the quiescence start time
    //!
    //! Reads the quiescence start time from the quiescence start time file. If the read fails, the current time is
    //! written to the file and returned.
    //!
    //! \warning this function will modify the quiescence start time file on disk if it does not already exist.
    //!
    //! \return The quiescence start time
    Fw::Time update_quiescence_start();

    // \brief get the system uptime
    //!
    Fw::Time get_uptime();

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

    //! Handler implementation for sequenceStarted
    void sequenceStarted_handler(FwIndexType portNum,            //!< The port number
                                 const Fw::StringBase& fileName  //!< The file path for start-up sequence
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

    //! Handler implementation for command GET_BOOT_COUNT
    //!
    //! Command to output the current boot count
    void GET_BOOT_COUNT_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                   U32 cmdSeq            //!< The command sequence number
                                   ) override;

  private:
    Fw::Time m_quiescence_start;             //!< Time of the start of the quiescence wait
    FwOpcodeType m_stored_opcode;            //!< Stored opcode for delayed response
    FwSizeType m_boot_count;                 //!< Current boot count
    bool m_boot_count_persisted = false;     //!< Whether the incremented boot count has reached the file
    bool m_boot_count_write_logged = false;  //!< Warning already emitted for the current persist-failure streak
    U32 m_stored_sequence;                   //!< Stored sequence number for delayed response
    std::atomic<bool> m_waiting;             //!< Indicates if waiting for quiescence
    Fw::String m_sequence_file;              //!< The filepath for the sequence last initiated
};

}  // namespace Components

#endif
