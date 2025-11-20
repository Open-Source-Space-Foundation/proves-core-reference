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

    //! Enter safe mode with optional reason override
    void enterSafeMode(const char* reason = nullptr);

    //! Exit safe mode
    void exitSafeMode();

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
    enum class SystemMode : U8 { NORMAL = 0, SAFE_MODE = 1 };

    //! Persistent state structure
    struct PersistentState {
        U8 mode;                 //!< Current mode (SystemMode)
        U32 safeModeEntryCount;  //!< Number of times safe mode entered
    };

    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    SystemMode m_mode;         //!< Current system mode
    U32 m_safeModeEntryCount;  //!< Counter for safe mode entries
    F32 m_currentVoltage;      //!< Current system voltage
    U32 m_runCounter;          //!< Counter for run handler calls (1Hz)

    static constexpr const char* STATE_FILE_PATH = "/mode_state.bin";  //!< State file path
};

}  // namespace Components

#endif
