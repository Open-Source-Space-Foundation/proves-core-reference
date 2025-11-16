// ======================================================================
// \title  ModeManager.hpp
// \author Auto-generated
// \brief  hpp file for ModeManager component implementation class
// ======================================================================

#ifndef Components_ModeManager_HPP
#define Components_ModeManager_HPP

#include <Os/File.hpp>

#include "FprimeZephyrReference/Components/ModeManager/ModeManagerComponentAc.hpp"
#include "Fw/Types/String.hpp"

namespace Components {

class ModeManager : public ModeManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct ModeManager object
    ModeManager(const char* const compName  //!< The component name
    );

    //! Destroy ModeManager object
    ~ModeManager();

    //! Initialize the component
    void init(FwSizeType queueDepth,        //!< Queue depth for async ports
              FwEnumStoreType instance = 0  //!< Instance ID
    );

    //! Preamble function called before scheduler starts
    void preamble();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for user-defined typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    //!
    //! Port receiving calls from the rate group (1Hz)
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    //! Handler implementation for watchdogFaultSignal
    //!
    //! Port to receive watchdog fault signal on boot
    void watchdogFaultSignal_handler(FwIndexType portNum  //!< The port number
                                     ) override;

    //! Handler implementation for commHeartbeat
    //!
    //! Port to receive ground communication heartbeat
    void commHeartbeat_handler(FwIndexType portNum  //!< The port number
                               ) override;

    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command CLEAR_VOLTAGE_FAULT
    void CLEAR_VOLTAGE_FAULT_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                        U32 cmdSeq            //!< The command sequence number
                                        ) override;

    //! Handler implementation for command CLEAR_WATCHDOG_FAULT
    void CLEAR_WATCHDOG_FAULT_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                         U32 cmdSeq            //!< The command sequence number
                                         ) override;

    //! Handler implementation for command CLEAR_COMM_FAULT
    void CLEAR_COMM_FAULT_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                     U32 cmdSeq            //!< The command sequence number
                                     ) override;

    //! Handler implementation for command FORCE_SAFE_MODE
    void FORCE_SAFE_MODE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                    U32 cmdSeq            //!< The command sequence number
                                    ) override;

    //! Handler implementation for command SET_COMM_TIMEOUT
    void SET_COMM_TIMEOUT_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                     U32 cmdSeq,           //!< The command sequence number
                                     U32 timeoutSeconds    //!< Timeout in seconds
                                     ) override;

    //! Handler implementation for command SET_VOLTAGE_THRESHOLDS
    void SET_VOLTAGE_THRESHOLDS_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                           U32 cmdSeq,           //!< The command sequence number
                                           F32 entryVoltage,     //!< Entry voltage
                                           F32 exitVoltage       //!< Exit voltage
                                           ) override;

  private:
    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Load persistent state from file
    void loadState();

    //! Save persistent state to file
    void saveState();

    //! Check voltage condition and update fault flag
    void checkVoltageCondition();

    //! Check communication timeout and update fault flag
    void checkCommTimeout();

    //! Enter safe mode
    void enterSafeMode();

    //! Exit safe mode
    void exitSafeMode();

    //! Update beacon backoff divider
    void updateBeaconBackoff();

    //! Turn off non-critical components
    void turnOffNonCriticalComponents();

    //! Turn on components (restore normal operation)
    void turnOnComponents();

    //! Get current voltage from PowerMonitor
    //! Note: In a real implementation, this would subscribe to telemetry
    //! For now, we'll use a simulated approach
    F32 getCurrentVoltage();

    // ----------------------------------------------------------------------
    // Private enums and types
    // ----------------------------------------------------------------------

    //! System mode enumeration
    enum class SystemMode : U8 { NORMAL = 0, SAFE_MODE = 1 };

    //! Persistent state structure
    struct PersistentState {
        U8 mode;                    //!< Current mode (SystemMode)
        bool voltageFaultFlag;      //!< Voltage fault flag
        bool watchdogFaultFlag;     //!< Watchdog fault flag
        bool commTimeoutFaultFlag;  //!< Communication timeout fault flag
        U32 safeModeEntryCount;     //!< Number of times safe mode entered
        U32 lastCommTimestamp;      //!< Last communication timestamp
        U32 backoffCounter;         //!< Beacon backoff counter
    };

    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    SystemMode m_mode;            //!< Current system mode
    bool m_voltageFaultFlag;      //!< Voltage fault flag
    bool m_watchdogFaultFlag;     //!< Watchdog fault flag
    bool m_commTimeoutFaultFlag;  //!< Communication timeout fault flag
    U32 m_safeModeEntryCount;     //!< Counter for safe mode entries
    U32 m_lastCommTimestamp;      //!< Timestamp of last communication
    U32 m_backoffCounter;         //!< Beacon backoff counter
    F32 m_currentVoltage;         //!< Current system voltage
    U32 m_runCounter;             //!< Counter for run handler calls (1Hz)
    bool m_initialized;           //!< Initialization flag

    static constexpr const char* STATE_FILE_PATH = "/mode_state.bin";  //!< State file path
};

}  // namespace Components

#endif
