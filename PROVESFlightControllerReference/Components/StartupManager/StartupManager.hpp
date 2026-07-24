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

    //! \brief read and increment the boot count
    //!
    //! Reads the boot count from the boot count file, increments it, and writes it back to the file. If the read
    //! fails, the boot count will be initialized to 1. If the write fails, a warning will be emitted.
    //!
    //! \warning this function will modify the boot count file on disk.
    //!
    //! \return The updated boot count
    FwSizeType get_boot_count(bool increment);

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

    //! Handler implementation for startupCompleteSequence
    void startupCompleteSequence_handler(FwIndexType portNum,             //!< The port number
                                         FwOpcodeType opCode,             //!< Command Op Code
                                         U32 cmdSeq,                      //!< Command Sequence
                                         const Fw::CmdResponse& response  //!< The command response argument
                                         ) override;

    //! Handler implementation for startupsequenceStarted
    void startupsequenceStarted_handler(FwIndexType portNum,            //!< The port number
                                        const Fw::StringBase& fileName  //!< The file path for start-up sequence
                                        ) override;

    //! Handler implementation for safeModeCompleteSequence
    void safeModeCompleteSequence_handler(FwIndexType portNum,             //!< The port number
                                          FwOpcodeType opCode,             //!< Command Op Code
                                          U32 cmdSeq,                      //!< Command Sequence
                                          const Fw::CmdResponse& response  //!< The command response argument
                                          ) override;

    //! Handler implementation for safeModeSequenceStarted
    void safeModeSequenceStarted_handler(FwIndexType portNum,            //!< The port number
                                         const Fw::StringBase& fileName  //!< The sequence file
                                         ) override;

    //! Handler implementation for payloadCompleteSequence
    void payloadCompleteSequence_handler(FwIndexType portNum,             //!< The port number
                                         FwOpcodeType opCode,             //!< Command Op Code
                                         U32 cmdSeq,                      //!< Command Sequence
                                         const Fw::CmdResponse& response  //!< The command response argument
                                         ) override;

    //! Handler implementation for payloadSequenceStarted
    void payloadSequenceStarted_handler(FwIndexType portNum,            //!< The port number
                                        const Fw::StringBase& fileName  //!< The sequence file
                                        ) override;

    //! Handler implementation for loraEverOn
    void loraEverOn_handler(FwIndexType portNum  //!< The port number
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

    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Record that a sequence has started on any sequencer
    void onSequenceStarted(const Fw::StringBase& fileName);

    //! Record that a sequence has completed on any sequencer
    void onSequenceCompleted();

    //! Assert enableTransmit if hardcoded enable is pending and safe to fire
    void tryHardcodedTransmitEnable();

  private:
    Fw::Time m_quiescence_start;              //!< Time of the start of the quiescence wait
    FwOpcodeType m_stored_opcode;             //!< Stored opcode for delayed response
    FwSizeType m_boot_count;                  //!< Current boot count
    U32 m_stored_sequence;                    //!< Stored sequence number for delayed response
    std::atomic<bool> m_waiting;              //!< Indicates if waiting for quiescence
    Fw::String m_sequence_file;               //!< The filepath for the sequence last initiated
    U32 m_transmit_enable_ticks = 0;          //!< Remaining 1 Hz ticks until hard-coded transmit enable
    U32 m_active_sequences = 0;               //!< Count of sequences currently running across sequencers
    bool m_pending_hardcoded_enable = false;  //!< Hardcoded enable deferred until sequences finish
    bool m_lora_ever_on = false;              //!< True once LoRa TX has been enabled this boot
};

}  // namespace Components

#endif
