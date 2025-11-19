// ======================================================================
// \title  ModeManager.cpp
// \author Auto-generated
// \brief  cpp file for ModeManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/ModeManager/ModeManager.hpp"

#include <algorithm>
#include <cstring>

#include "Fw/Types/Assert.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ModeManager ::ModeManager(const char* const compName)
    : ModeManagerComponentBase(compName),
      m_mode(SystemMode::NORMAL),
      m_safeModeEntryCount(0),
      m_currentVoltage(0.0f),
      m_runCounter(0) {}

ModeManager ::~ModeManager() {}

void ModeManager ::init(FwSizeType queueDepth, FwEnumStoreType instance) {
    ModeManagerComponentBase::init(queueDepth, instance);
}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

void ModeManager ::run_handler(FwIndexType portNum, U32 context) {
    // Increment run counter (1Hz tick counter)
    this->m_runCounter++;

    // Wait 5 seconds after boot before checking voltage
    // This allows INA219 and other hardware to fully initialize
    if (this->m_runCounter < 5) {
        return;  // Skip voltage checking for first 5 seconds
    }

    // 1. Check voltage condition (telemetry only)
    this->checkVoltageCondition();

    // 2. Update telemetry
    this->tlmWrite_CurrentMode(static_cast<U8>(this->m_mode));
    this->tlmWrite_CurrentVoltage(this->m_currentVoltage);
}

void ModeManager ::forceSafeMode_handler(FwIndexType portNum) {
    // Force entry into safe mode (called by other components)
    // Provides immediate safe mode entry for critical component-detected faults
    this->log_WARNING_HI_ExternalFaultDetected();

    if (this->m_mode == SystemMode::NORMAL) {
        this->enterSafeMode("External component request");
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void ModeManager ::FORCE_SAFE_MODE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Force entry into safe mode
    this->log_ACTIVITY_HI_ManualSafeModeEntry();

    if (this->m_mode == SystemMode::NORMAL) {
        this->enterSafeMode("Ground command");
    }

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void ModeManager ::EXIT_SAFE_MODE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Manual command to exit safe mode

    // Check if currently in safe mode
    if (this->m_mode != SystemMode::SAFE_MODE) {
        Fw::LogStringArg cmdNameStr("EXIT_SAFE_MODE");
        Fw::LogStringArg reasonStr("Not currently in safe mode");
        this->log_WARNING_LO_CommandValidationFailed(cmdNameStr, reasonStr);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    // All validations passed - exit safe mode
    this->exitSafeMode();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

void ModeManager ::loadState() {
    Os::File file;
    Os::File::Status status = file.open(STATE_FILE_PATH, Os::File::OPEN_READ);

    if (status == Os::File::OP_OK) {
        PersistentState state;
        FwSizeType size = sizeof(PersistentState);
        status = file.read(reinterpret_cast<U8*>(&state), size, Os::File::WaitType::WAIT);

        if (status == Os::File::OP_OK && size == sizeof(PersistentState)) {
            // Validate state data before restoring
            if (state.mode <= static_cast<U8>(SystemMode::SAFE_MODE)) {
                // Valid mode value - restore state
                this->m_mode = static_cast<SystemMode>(state.mode);
                this->m_safeModeEntryCount = state.safeModeEntryCount;
            } else {
                // Corrupted state - use defaults
                this->m_mode = SystemMode::NORMAL;
                this->m_safeModeEntryCount = 0;
            }
        }

        file.close();
    } else {
        // File doesn't exist or can't be opened - initialize to default state
        this->m_mode = SystemMode::NORMAL;
        this->m_safeModeEntryCount = 0;
    }
}

void ModeManager ::saveState() {
    Os::File file;
    Os::File::Status status = file.open(STATE_FILE_PATH, Os::File::OPEN_CREATE);
    if (status == Os::File::OP_OK) {
        PersistentState state;
        state.mode = static_cast<U8>(this->m_mode);
        state.safeModeEntryCount = this->m_safeModeEntryCount;

        FwSizeType size = sizeof(PersistentState);
        Os::File::Status writeStatus = file.write(reinterpret_cast<U8*>(&state), size, Os::File::WaitType::WAIT);

        // Verify write succeeded and correct number of bytes written
        FW_ASSERT(writeStatus == Os::File::OP_OK && size == sizeof(PersistentState),
                  static_cast<FwAssertArgType>(writeStatus));

        file.close();
    }
}

void ModeManager ::checkVoltageCondition() {
    // Get current voltage
    bool voltageValid = false;
    F32 voltage = this->getCurrentVoltage(voltageValid);

    // Update telemetry only if reading is valid
    if (voltageValid) {
        this->m_currentVoltage = voltage;
    }
}

void ModeManager ::enterSafeMode(const char* reasonOverride) {
    // Transition to safe mode
    this->m_mode = SystemMode::SAFE_MODE;
    this->m_safeModeEntryCount++;

    // Build reason string
    Fw::LogStringArg reasonStr;
    char reasonBuf[100];
    if (reasonOverride != nullptr) {
        reasonStr = reasonOverride;
    } else {
        snprintf(reasonBuf, sizeof(reasonBuf), "Unknown");
        reasonStr = reasonBuf;
    }

    this->log_WARNING_HI_EnteringSafeMode(reasonStr);

    // Turn off non-critical components
    this->turnOffNonCriticalComponents();

    // Update telemetry
    this->tlmWrite_CurrentMode(static_cast<U8>(this->m_mode));
    this->tlmWrite_SafeModeEntryCount(this->m_safeModeEntryCount);

    // Notify other components of mode change
    if (this->isConnected_modeChanged_OutputPort(0)) {
        this->modeChanged_out(0);
    }

    // Save state
    this->saveState();
}

void ModeManager ::exitSafeMode() {
    // Transition back to normal mode
    this->m_mode = SystemMode::NORMAL;

    this->log_ACTIVITY_HI_ExitingSafeMode();

    // Turn on components (restore normal operation)
    this->turnOnComponents();

    // Update telemetry
    this->tlmWrite_CurrentMode(static_cast<U8>(this->m_mode));

    // Notify other components of mode change
    if (this->isConnected_modeChanged_OutputPort(0)) {
        this->modeChanged_out(0);
    }

    // Save state
    this->saveState();
}

void ModeManager ::turnOffNonCriticalComponents() {
    // Turn OFF:
    // - Satellite faces 0-5 (LoadSwitch instances 0-5)
    // - Payload power (LoadSwitch instance 6)
    // - Payload battery (LoadSwitch instance 7)

    // Send turn off signal to all 8 load switches
    for (FwIndexType i = 0; i < 8; i++) {
        if (this->isConnected_loadSwitchTurnOff_OutputPort(i)) {
            this->loadSwitchTurnOff_out(i);
        }
    }

    // Note: IMU and antenna deployer control can be added when needed
}

void ModeManager ::turnOnComponents() {
    // Turn ON all load switches to restore normal operation

    // Send turn on signal to all 8 load switches
    for (FwIndexType i = 0; i < 8; i++) {
        if (this->isConnected_loadSwitchTurnOn_OutputPort(i)) {
            this->loadSwitchTurnOn_out(i);
        }
    }
}

F32 ModeManager ::getCurrentVoltage(bool& valid) {
    // Call the voltage get port to get current system voltage
    if (this->isConnected_voltageGet_OutputPort(0)) {
        F64 voltage = this->voltageGet_out(0);
        valid = true;
        return static_cast<F32>(voltage);  // Convert from F64 to F32
    }

    // Port is not connected - voltage reading is INVALID
    // Do NOT return a fake value that could mask a real brown-out condition
    valid = false;
    return 0.0f;
}

}  // namespace Components
