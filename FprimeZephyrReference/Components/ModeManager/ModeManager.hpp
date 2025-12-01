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
    //! @param reason The reason for entering safe mode (NONE defaults to EXTERNAL_REQUEST)
    void forceSafeMode_handler(FwIndexType portNum,                      //!< The port number
                               const Components::SafeModeReason& reason  //!< The safe mode reason
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
    void exitSafeModeAutomatic(F32 voltage);

    //! Turn off non-critical components
    void turnOffNonCriticalComponents();

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

    //! System mode enumeration
    enum class SystemMode : U8 { SAFE_MODE = 1, NORMAL = 2 };

    //! Persistent state structure
    struct PersistentState {
        U8 mode;                 //!< Current mode (SystemMode)
        U32 safeModeEntryCount;  //!< Number of times safe mode entered
        U8 safeModeReason;       //!< Reason for safe mode entry (SafeModeReason)
        U8 cleanShutdown;        //!< Clean shutdown flag (1 = clean, 0 = unclean)
    };

    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    SystemMode m_mode;                            //!< Current system mode
    U32 m_safeModeEntryCount;                     //!< Counter for safe mode entries
    U32 m_runCounter;                             //!< Counter for run handler calls (1Hz)
    Components::SafeModeReason m_safeModeReason;  //!< Current safe mode reason
    U32 m_safeModeVoltageCounter;                 //!< Counter for low voltage in NORMAL mode
    U32 m_recoveryVoltageCounter;                 //!< Counter for voltage recovery in SAFE_MODE

    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    static constexpr const char* STATE_FILE_PATH = "/mode_state.bin";  //!< State file path
    static constexpr F32 SAFE_MODE_ENTRY_VOLTAGE = 6.7f;               //!< Voltage threshold for safe mode entry (V)
    static constexpr F32 SAFE_MODE_RECOVERY_VOLTAGE = 8.0f;            //!< Voltage threshold for safe mode recovery (V)
    static constexpr U32 SAFE_MODE_DEBOUNCE_SECONDS = 10;              //!< Debounce time for voltage transitions (s)
};

}  // namespace Components

#endif
