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

    //! Handler implementation for forceSafeMode
    //!
    //! Port to force safe mode entry (callable by other components)
    void forceSafeMode_handler(FwIndexType portNum  //!< The port number
                               ) override;

    //! Handler implementation for getMode
    //!
    //! Port to query the current system mode
    Components::SystemMode getMode_handler(FwIndexType portNum  //!< The port number
                                           ) override;

    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command FORCE_SAFE_MODE
    void FORCE_SAFE_MODE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                    U32 cmdSeq            //!< The command sequence number
                                    ) override;

    //! Handler implementation for command EXIT_SAFE_MODE
    void EXIT_SAFE_MODE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                   U32 cmdSeq            //!< The command sequence number
                                   ) override;

    //! Handler implementation for command ENTER_PAYLOAD_MODE
    void ENTER_PAYLOAD_MODE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                       U32 cmdSeq            //!< The command sequence number
                                       ) override;

    //! Handler implementation for command EXIT_PAYLOAD_MODE
    void EXIT_PAYLOAD_MODE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                      U32 cmdSeq            //!< The command sequence number
                                      ) override;

    //! Handler implementation for command ENTER_HIBERNATION
    void ENTER_HIBERNATION_cmdHandler(FwOpcodeType opCode,   //!< The opcode
                                      U32 cmdSeq,            //!< The command sequence number
                                      U32 sleepDurationSec,  //!< Sleep cycle duration in seconds
                                      U32 wakeDurationSec    //!< Wake window duration in seconds
                                      ) override;

    //! Handler implementation for command EXIT_HIBERNATION
    void EXIT_HIBERNATION_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                     U32 cmdSeq            //!< The command sequence number
                                     ) override;

  private:
    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Load persistent state from file
    void loadState();

    //! Save persistent state to file
    void saveState();

    //! Enter safe mode with optional reason override
    void enterSafeMode(const char* reason = nullptr);

    //! Exit safe mode
    void exitSafeMode();

    //! Enter payload mode with optional reason override
    void enterPayloadMode(const char* reason = nullptr);

    //! Exit payload mode
    void exitPayloadMode();

    //! Turn off non-critical components
    void turnOffNonCriticalComponents();

    //! Turn on payload (load switches 6 & 7)
    void turnOnPayload();

    //! Turn off payload (load switches 6 & 7)
    void turnOffPayload();

    //! Turn on components (restore normal operation)
    void turnOnComponents();

    //! Get current voltage from INA219 system power manager
    //! Queries voltage via the voltageGet output port
    //! \param valid Output parameter indicating if the voltage reading is valid
    //! \return Current voltage (only valid if valid parameter is set to true)
    F32 getCurrentVoltage(bool& valid);

    //! Enter hibernation mode with configurable durations
    //! \param sleepDurationSec Duration of each sleep cycle in seconds
    //! \param wakeDurationSec Duration of each wake window in seconds
    //! \param reason Reason for entering hibernation
    void enterHibernation(U32 sleepDurationSec, U32 wakeDurationSec, const char* reason = nullptr);

    //! Exit hibernation mode (transitions to SAFE_MODE)
    void exitHibernation();

    //! Enter dormant sleep (calls PicoSleep, does not return on success)
    void enterDormantSleep();

    //! Start wake window after dormant wake
    void startWakeWindow();

    //! Handle wake window tick (called from run_handler at 1Hz)
    void handleWakeWindowTick();

    // ----------------------------------------------------------------------
    // Private enums and types
    // ----------------------------------------------------------------------

    //! System mode enumeration
    enum class SystemMode : U8 {
        HIBERNATION_MODE = 0,  //!< Ultra-low-power hibernation with periodic wake windows
        SAFE_MODE = 1,         //!< Safe mode with non-critical components powered off
        NORMAL = 2,            //!< Normal operational mode
        PAYLOAD_MODE = 3       //!< Payload mode with payload power and battery enabled
    };

    //! Persistent state structure (version 2 with hibernation support)
    struct PersistentState {
        U8 mode;                      //!< Current mode (SystemMode)
        U32 safeModeEntryCount;       //!< Number of times safe mode entered
        U32 payloadModeEntryCount;    //!< Number of times payload mode entered
        U32 hibernationCycleCount;    //!< Number of hibernation sleep/wake cycles
        U32 hibernationTotalSeconds;  //!< Total time spent in hibernation (seconds)
        U32 sleepDurationSec;         //!< Configured sleep cycle duration (for resume)
        U32 wakeDurationSec;          //!< Configured wake window duration (for resume)
    };

    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    SystemMode m_mode;            //!< Current system mode
    U32 m_safeModeEntryCount;     //!< Counter for safe mode entries
    U32 m_payloadModeEntryCount;  //!< Counter for payload mode entries
    U32 m_runCounter;             //!< Counter for run handler calls (1Hz)

    // Hibernation state variables
    bool m_inHibernationWakeWindow;  //!< True if currently in wake window
    U32 m_wakeWindowCounter;         //!< Seconds elapsed in current wake window
    U32 m_hibernationCycleCount;     //!< Total hibernation cycles completed
    U32 m_hibernationTotalSeconds;   //!< Total seconds spent in hibernation
    U32 m_sleepDurationSec;          //!< Configured sleep cycle duration
    U32 m_wakeDurationSec;           //!< Configured wake window duration

    static constexpr const char* STATE_FILE_PATH = "/mode_state.bin";  //!< State file path
};

}  // namespace Components

#endif
