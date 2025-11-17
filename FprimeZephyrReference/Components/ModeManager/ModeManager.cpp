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
      m_voltageFaultFlag(false),
      m_watchdogFaultFlag(false),
      m_externalFaultFlag(false),
      m_safeModeEntryCount(0),
      m_currentVoltage(0.0f),
      m_runCounter(0),
      m_initialized(false) {}

ModeManager ::~ModeManager() {}

void ModeManager ::init(FwSizeType queueDepth, FwEnumStoreType instance) {
    ModeManagerComponentBase::init(queueDepth, instance);
}

void ModeManager ::preamble() {
    // Load persistent state from file
    this->loadState();
    this->m_initialized = true;

    // Emit initial telemetry
    this->tlmWrite_CurrentMode(static_cast<U8>(this->m_mode));
    this->tlmWrite_VoltageFaultFlag(this->m_voltageFaultFlag);
    this->tlmWrite_WatchdogFaultFlag(this->m_watchdogFaultFlag);
    this->tlmWrite_ExternalFaultFlag(this->m_externalFaultFlag);
    this->tlmWrite_SafeModeEntryCount(this->m_safeModeEntryCount);
}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

void ModeManager ::run_handler(FwIndexType portNum, U32 context) {
    if (!this->m_initialized) {
        return;
    }

    // Increment run counter (1Hz tick counter)
    this->m_runCounter++;

    // Wait 5 seconds after boot before checking voltage
    // This allows INA219 and other hardware to fully initialize
    if (this->m_runCounter < 5) {
        return;  // Skip voltage checking for first 5 seconds
    }

    // 1. Check voltage condition
    this->checkVoltageCondition();

    // 2. Determine if mode change is needed (entry only - exit requires manual command)
    bool anyFaultActive = (this->m_voltageFaultFlag || this->m_watchdogFaultFlag || this->m_externalFaultFlag);

    if (anyFaultActive && this->m_mode == SystemMode::NORMAL) {
        this->enterSafeMode();
    }
    // Note: Safe mode exit requires manual EXIT_SAFE_MODE command
    // This ensures operator awareness of all mode changes and prevents boot loops

    // 3. Update telemetry
    this->tlmWrite_CurrentMode(static_cast<U8>(this->m_mode));
    this->tlmWrite_VoltageFaultFlag(this->m_voltageFaultFlag);
    this->tlmWrite_WatchdogFaultFlag(this->m_watchdogFaultFlag);
    this->tlmWrite_ExternalFaultFlag(this->m_externalFaultFlag);
    this->tlmWrite_CurrentVoltage(this->m_currentVoltage);
}

void ModeManager ::watchdogFaultSignal_handler(FwIndexType portNum) {
    // Watchdog fault detected (signal port - no parameters)
    this->m_watchdogFaultFlag = true;
    this->log_WARNING_HI_WatchdogFaultDetected();
    this->tlmWrite_WatchdogFaultFlag(true);

    // Save state immediately
    this->saveState();
}

void ModeManager ::forceSafeMode_handler(FwIndexType portNum) {
    // Force entry into safe mode (called by other components)
    // Provides immediate safe mode entry for critical component-detected faults
    this->m_externalFaultFlag = true;
    this->log_WARNING_HI_ExternalFaultDetected();
    this->tlmWrite_ExternalFaultFlag(true);
    this->saveState();

    if (this->m_mode == SystemMode::NORMAL) {
        this->enterSafeMode("External component request");
    }
}

void ModeManager ::clearAllFaults_handler(FwIndexType portNum) {
    // Clear all fault flags (called by other components)
    //
    // ⚠️ SAFETY WARNING ⚠️
    // This port handler bypasses all validation checks that the command handlers perform:
    // - Does NOT verify voltage > 7.5V before clearing voltage fault
    // - Does NOT check if system is actually ready to recover
    // - Clears faults unconditionally without operator approval
    //
    // USE WITH EXTREME CAUTION
    // This port is intended ONLY for automated recovery scenarios where:
    // 1. The calling component has already verified system health
    // 2. Immediate fault clearing is required for autonomous operation
    // 3. The risk of premature recovery has been assessed
    //
    // For normal operations, prefer using ground commands:
    // - CLEAR_VOLTAGE_FAULT (validates voltage > 7.5V)
    // - CLEAR_WATCHDOG_FAULT (manual operator approval)
    // - CLEAR_EXTERNAL_FAULT (manual operator approval)
    //
    this->m_voltageFaultFlag = false;
    this->m_watchdogFaultFlag = false;
    this->m_externalFaultFlag = false;
    this->tlmWrite_VoltageFaultFlag(false);
    this->tlmWrite_WatchdogFaultFlag(false);
    this->tlmWrite_ExternalFaultFlag(false);
    this->saveState();
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void ModeManager ::CLEAR_VOLTAGE_FAULT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Get current voltage exit threshold parameter
    Fw::ParamValid paramValid;
    F32 exitThreshold = this->paramGet_VOLTAGE_EXIT_THRESHOLD(paramValid);
    if (paramValid != Fw::ParamValid::VALID) {
        exitThreshold = 7.5f;  // Default value
    }

    // Check if voltage reading is valid
    bool voltageValid = false;
    F32 currentVoltage = this->getCurrentVoltage(voltageValid);

    if (!voltageValid) {
        // CRITICAL: Voltage measurement unavailable - cannot verify safe conditions
        // Fail validation to prevent clearing fault with stale/fake data
        Fw::LogStringArg cmdNameStr("CLEAR_VOLTAGE_FAULT");
        Fw::LogStringArg reasonStr("Voltage measurement unavailable (INA219 disconnected or failed)");
        this->log_WARNING_LO_CommandValidationFailed(cmdNameStr, reasonStr);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    if (currentVoltage >= exitThreshold) {
        // Clear the fault flag
        this->m_voltageFaultFlag = false;
        this->log_ACTIVITY_HI_VoltageFaultCleared(currentVoltage);
        this->tlmWrite_VoltageFaultFlag(false);
        this->saveState();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    } else {
        // Validation failed - voltage still too low
        Fw::LogStringArg reasonStr;
        char reasonBuf[100];
        snprintf(reasonBuf, sizeof(reasonBuf), "Voltage %.2fV below threshold %.2fV",
                 static_cast<double>(currentVoltage), static_cast<double>(exitThreshold));
        reasonStr = reasonBuf;
        Fw::LogStringArg cmdNameStr("CLEAR_VOLTAGE_FAULT");
        this->log_WARNING_LO_CommandValidationFailed(cmdNameStr, reasonStr);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
    }
}

void ModeManager ::CLEAR_WATCHDOG_FAULT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Clear watchdog fault flag (no validation needed - manual clear only)
    this->m_watchdogFaultFlag = false;
    this->log_ACTIVITY_HI_WatchdogFaultCleared();
    this->tlmWrite_WatchdogFaultFlag(false);
    this->saveState();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void ModeManager ::CLEAR_EXTERNAL_FAULT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Clear external fault flag (no validation needed - manual clear only)
    this->m_externalFaultFlag = false;
    this->log_ACTIVITY_HI_ExternalFaultCleared();
    this->tlmWrite_ExternalFaultFlag(false);
    this->saveState();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void ModeManager ::FORCE_SAFE_MODE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Force entry into safe mode
    // Set external fault flag to require explicit CLEAR_EXTERNAL_FAULT before EXIT_SAFE_MODE
    this->m_externalFaultFlag = true;
    this->log_WARNING_HI_ExternalFaultDetected();
    this->tlmWrite_ExternalFaultFlag(true);
    this->saveState();

    if (this->m_mode == SystemMode::NORMAL) {
        this->enterSafeMode("Ground command");
    }

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void ModeManager ::EXIT_SAFE_MODE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Manual command to exit safe mode
    // This is required after watchdog faults to prevent boot loops

    // Check if currently in safe mode
    if (this->m_mode != SystemMode::SAFE_MODE) {
        Fw::LogStringArg cmdNameStr("EXIT_SAFE_MODE");
        Fw::LogStringArg reasonStr("Not currently in safe mode");
        this->log_WARNING_LO_CommandValidationFailed(cmdNameStr, reasonStr);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    // Check that all faults are cleared before allowing exit
    bool anyFaultActive = (this->m_voltageFaultFlag || this->m_watchdogFaultFlag || this->m_externalFaultFlag);
    if (anyFaultActive) {
        Fw::LogStringArg cmdNameStr("EXIT_SAFE_MODE");
        Fw::LogStringArg reasonStr;
        char reasonBuf[100];
        char* ptr = reasonBuf;
        int remaining = sizeof(reasonBuf);

        snprintf(ptr, remaining, "Active faults: ");
        ptr += strlen(ptr);
        remaining = sizeof(reasonBuf) - strlen(reasonBuf);

        if (this->m_voltageFaultFlag) {
            snprintf(ptr, remaining, "Voltage ");
            ptr += strlen(ptr);
            remaining = sizeof(reasonBuf) - strlen(reasonBuf);
        }
        if (this->m_watchdogFaultFlag) {
            snprintf(ptr, remaining, "Watchdog ");
            ptr += strlen(ptr);
            remaining = sizeof(reasonBuf) - strlen(reasonBuf);
        }
        if (this->m_externalFaultFlag) {
            snprintf(ptr, remaining, "External");
        }

        reasonStr = reasonBuf;
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
            // Restore state
            this->m_mode = static_cast<SystemMode>(state.mode);
            this->m_voltageFaultFlag = state.voltageFaultFlag;
            this->m_watchdogFaultFlag = state.watchdogFaultFlag;
            this->m_externalFaultFlag = state.externalFaultFlag;
            this->m_safeModeEntryCount = state.safeModeEntryCount;
        }

        file.close();
    } else {
        // File doesn't exist or can't be opened - initialize to default state
        this->m_mode = SystemMode::NORMAL;
        this->m_voltageFaultFlag = false;
        this->m_watchdogFaultFlag = false;
        this->m_externalFaultFlag = false;
        this->m_safeModeEntryCount = 0;
    }
}

void ModeManager ::saveState() {
    Os::File file;
    Os::File::Status status = file.open(STATE_FILE_PATH, Os::File::OPEN_WRITE);

    if (status != Os::File::OP_OK) {
        // Try to create the file
        status = file.open(STATE_FILE_PATH, Os::File::OPEN_CREATE);
    }

    if (status == Os::File::OP_OK) {
        PersistentState state;
        state.mode = static_cast<U8>(this->m_mode);
        state.voltageFaultFlag = this->m_voltageFaultFlag;
        state.watchdogFaultFlag = this->m_watchdogFaultFlag;
        state.externalFaultFlag = this->m_externalFaultFlag;
        state.safeModeEntryCount = this->m_safeModeEntryCount;

        FwSizeType size = sizeof(PersistentState);
        file.write(reinterpret_cast<U8*>(&state), size, Os::File::WaitType::WAIT);
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

    // Get voltage entry threshold parameter
    Fw::ParamValid paramValid;
    F32 entryThreshold = this->paramGet_VOLTAGE_ENTRY_THRESHOLD(paramValid);
    if (paramValid != Fw::ParamValid::VALID) {
        entryThreshold = 7.0f;  // Default value
    }

    // SAFETY: If voltage measurement is unavailable, treat it as a fault condition
    // This prevents the system from running blind without power monitoring
    if (!voltageValid) {
        if (!this->m_voltageFaultFlag) {
            this->m_voltageFaultFlag = true;
            this->log_WARNING_HI_VoltageFaultDetected(0.0f);  // Log with 0.0f to indicate invalid reading
            this->tlmWrite_VoltageFaultFlag(true);
            this->saveState();
        }
        return;
    }

    // Check if voltage has dropped below entry threshold
    if (!this->m_voltageFaultFlag && voltage < entryThreshold) {
        // Set fault flag
        this->m_voltageFaultFlag = true;
        this->log_WARNING_HI_VoltageFaultDetected(voltage);
        this->tlmWrite_VoltageFaultFlag(true);
        this->saveState();
    }
}

void ModeManager ::enterSafeMode(const char* reasonOverride) {
    // Transition to safe mode
    this->m_mode = SystemMode::SAFE_MODE;
    this->m_safeModeEntryCount++;

    // Build reason string
    Fw::LogStringArg reasonStr;
    char reasonBuf[100];
    reasonBuf[0] = '\0';
    if (reasonOverride != nullptr) {
        reasonStr = reasonOverride;
    } else {
        char* ptr = reasonBuf;
        int remaining = sizeof(reasonBuf);
        bool wroteReason = false;

        if (this->m_voltageFaultFlag) {
            int written = snprintf(ptr, remaining, "Low voltage");
            ptr += written;
            remaining -= written;
            wroteReason = true;
        }
        if (this->m_watchdogFaultFlag) {
            if (wroteReason) {
                int written = snprintf(ptr, remaining, ", ");
                ptr += written;
                remaining -= written;
            }
            int written = snprintf(ptr, remaining, "Watchdog fault");
            ptr += written;
            remaining -= written;
            wroteReason = true;
        }
        if (this->m_externalFaultFlag) {
            if (wroteReason) {
                int written = snprintf(ptr, remaining, ", ");
                ptr += written;
                remaining -= written;
            }
            snprintf(ptr, remaining, "External fault");
            wroteReason = true;
        }

        if (!wroteReason) {
            snprintf(reasonBuf, sizeof(reasonBuf), "Unknown trigger");
        }

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
