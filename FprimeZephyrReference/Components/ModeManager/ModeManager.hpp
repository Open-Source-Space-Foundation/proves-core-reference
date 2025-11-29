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

    //! Handler implementation for prepareForReboot
    //!
    //! Port called before intentional reboot to set clean shutdown flag
    void prepareForReboot_handler(FwIndexType portNum  //!< The port number
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

  private:
    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Load persistent state from file
    void loadState();

    //! Save persistent state to file
    void saveState();

    //! Enter safe mode with specified reason
    void enterSafeMode(Components::SafeModeReason reason);

    //! Exit safe mode (manual command)
    void exitSafeMode();

    //! Exit safe mode automatically due to voltage recovery
    //! Only allowed when safe mode reason is LOW_BATTERY
    void exitSafeModeAutomatic(F32 voltage);

    //! Enter payload mode with optional reason override
    void enterPayloadMode(const char* reason = nullptr);

    //! Exit payload mode (manual)
    void exitPayloadMode();

    //! Exit payload mode automatically due to fault condition
    //! More aggressive than manual exit - turns off all switches
    void exitPayloadModeAutomatic(F32 voltage);

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

    // ----------------------------------------------------------------------
    // Private enums and types
    // ----------------------------------------------------------------------

    //! System mode enumeration (values ordered for +1/-1 sequential transitions)
    enum class SystemMode : U8 { SAFE_MODE = 1, NORMAL = 2, PAYLOAD_MODE = 3 };

    //! Persistent state structure (v2: includes safe mode reason and clean shutdown flag)
    struct PersistentState {
        U8 mode;                    //!< Current mode (SystemMode)
        U32 safeModeEntryCount;     //!< Number of times safe mode entered
        U32 payloadModeEntryCount;  //!< Number of times payload mode entered
        U8 safeModeReason;          //!< Reason for current safe mode (SafeModeReason)
        U8 cleanShutdown;           //!< Flag indicating if last shutdown was intentional (1=clean, 0=unclean)
    };

    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    SystemMode m_mode;            //!< Current system mode
    U32 m_safeModeEntryCount;     //!< Counter for safe mode entries
    U32 m_payloadModeEntryCount;  //!< Counter for payload mode entries
    U32 m_runCounter;             //!< Counter for run handler calls (1Hz)
    U32 m_lowVoltageCounter;      //!< Counter for consecutive low voltage readings (payload mode exit)

    // Safe mode specific state
    Components::SafeModeReason m_safeModeReason;  //!< Reason for current safe mode entry
    U32 m_safeModeVoltageCounter;                 //!< Counter for consecutive low voltage readings (safe mode entry)
    U32 m_recoveryVoltageCounter;                 //!< Counter for consecutive high voltage readings (safe mode exit)

    static constexpr const char* STATE_FILE_PATH = "/mode_state.bin";  //!< State file path

    // Voltage threshold constants for payload mode protection
    static constexpr F32 LOW_VOLTAGE_THRESHOLD = 7.2f;       //!< Voltage threshold for payload mode exit
    static constexpr U32 LOW_VOLTAGE_DEBOUNCE_SECONDS = 10;  //!< Consecutive seconds below threshold

    // Voltage threshold constants for safe mode entry/exit (Normal <-> Safe)
    static constexpr F32 SAFE_MODE_ENTRY_VOLTAGE = 6.7f;     //!< Voltage threshold for safe mode entry
    static constexpr F32 SAFE_MODE_RECOVERY_VOLTAGE = 8.0f;  //!< Voltage threshold for safe mode auto-recovery
    static constexpr U32 SAFE_MODE_DEBOUNCE_SECONDS = 10;    //!< Consecutive seconds for safe mode transitions

    // Buffer size for reason strings (must match FPP string size definitions)
    static constexpr FwSizeType REASON_STRING_SIZE = 100;  //!< Matches FPP reason: string size 100
};

}  // namespace Components

#endif
