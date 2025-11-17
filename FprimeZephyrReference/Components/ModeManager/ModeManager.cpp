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
      m_safeModeEntryCount(0),
      m_backoffCounter(0),
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

    // 1. Check voltage condition
    this->checkVoltageCondition();

    // 2. Determine if mode change is needed
    bool anyFaultActive = (this->m_voltageFaultFlag || this->m_watchdogFaultFlag);

    if (anyFaultActive && this->m_mode == SystemMode::NORMAL) {
        this->enterSafeMode();
    }
    // Auto-exit removed - all safe mode exits require manual EXIT_SAFE_MODE command
    // This simplifies the logic and ensures operator awareness of all mode changes

    // 4. If in safe mode, update exponential backoff
    if (this->m_mode == SystemMode::SAFE_MODE) {
        this->updateBeaconBackoff();
    }

    // 3. Update telemetry
    this->tlmWrite_CurrentMode(static_cast<U8>(this->m_mode));
    this->tlmWrite_VoltageFaultFlag(this->m_voltageFaultFlag);
    this->tlmWrite_WatchdogFaultFlag(this->m_watchdogFaultFlag);
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
    if (this->m_mode == SystemMode::NORMAL) {
        // Set voltage fault flag to trigger safe mode
        this->m_voltageFaultFlag = true;
        this->log_WARNING_HI_VoltageFaultDetected(this->m_currentVoltage);
        this->tlmWrite_VoltageFaultFlag(true);
        this->saveState();
    }
}

void ModeManager ::clearAllFaults_handler(FwIndexType portNum) {
    // Clear all fault flags (called by other components)
    // Note: This doesn't validate conditions like the command handlers do
    // Use with caution - typically commands are preferred for safety
    this->m_voltageFaultFlag = false;
    this->m_watchdogFaultFlag = false;
    this->tlmWrite_VoltageFaultFlag(false);
    this->tlmWrite_WatchdogFaultFlag(false);
    this->saveState();
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void ModeManager ::CLEAR_VOLTAGE_FAULT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Get current voltage exit threshold parameter
    Fw::ParamValid valid;
    F32 exitThreshold = this->paramGet_VOLTAGE_EXIT_THRESHOLD(valid);
    if (valid != Fw::ParamValid::VALID) {
        exitThreshold = 7.5f;  // Default value
    }

    // Check if voltage is above exit threshold
    F32 currentVoltage = this->getCurrentVoltage();

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
        snprintf(reasonBuf, sizeof(reasonBuf), "Voltage %.2fV below threshold %.2fV", currentVoltage, exitThreshold);
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

void ModeManager ::FORCE_SAFE_MODE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Force entry into safe mode
    if (this->m_mode == SystemMode::NORMAL) {
        // Set a flag to force safe mode (we'll use voltage flag as the trigger)
        this->m_voltageFaultFlag = true;
        this->enterSafeMode();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    } else {
        // Already in safe mode
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    }
}

void ModeManager ::SET_VOLTAGE_THRESHOLDS_cmdHandler(FwOpcodeType opCode,
                                                     U32 cmdSeq,
                                                     F32 entryVoltage,
                                                     F32 exitVoltage) {
    // Validate that exit voltage is higher than entry voltage
    if (exitVoltage <= entryVoltage) {
        Fw::LogStringArg cmdNameStr("SET_VOLTAGE_THRESHOLDS");
        Fw::LogStringArg reasonStr("Exit voltage must be greater than entry voltage");
        this->log_WARNING_LO_CommandValidationFailed(cmdNameStr, reasonStr);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    // Note: In F Prime, parameters are typically loaded from files, not set programmatically
    // This command would need to update the parameter database file
    // For now, just acknowledge the command
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
    bool anyFaultActive = (this->m_voltageFaultFlag || this->m_watchdogFaultFlag);
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
            snprintf(ptr, remaining, "Watchdog");
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
            this->m_safeModeEntryCount = state.safeModeEntryCount;
            this->m_backoffCounter = state.backoffCounter;
        }

        file.close();
    } else {
        // File doesn't exist or can't be opened - initialize to default state
        this->m_mode = SystemMode::NORMAL;
        this->m_voltageFaultFlag = false;
        this->m_watchdogFaultFlag = false;
        this->m_safeModeEntryCount = 0;
        this->m_backoffCounter = 0;
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
        state.safeModeEntryCount = this->m_safeModeEntryCount;
        state.backoffCounter = this->m_backoffCounter;

        FwSizeType size = sizeof(PersistentState);
        file.write(reinterpret_cast<U8*>(&state), size, Os::File::WaitType::WAIT);
        file.close();
    }
}

void ModeManager ::checkVoltageCondition() {
    // Get current voltage
    F32 voltage = this->getCurrentVoltage();
    this->m_currentVoltage = voltage;

    // Get voltage entry threshold parameter
    Fw::ParamValid valid;
    F32 entryThreshold = this->paramGet_VOLTAGE_ENTRY_THRESHOLD(valid);
    if (valid != Fw::ParamValid::VALID) {
        entryThreshold = 7.0f;  // Default value
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

void ModeManager ::enterSafeMode() {
    // Transition to safe mode
    this->m_mode = SystemMode::SAFE_MODE;
    this->m_safeModeEntryCount++;
    this->m_backoffCounter = 0;  // Reset backoff counter

    // Build reason string
    Fw::LogStringArg reasonStr;
    char reasonBuf[100];
    char* ptr = reasonBuf;
    int remaining = sizeof(reasonBuf);

    if (this->m_voltageFaultFlag) {
        int written = snprintf(ptr, remaining, "Low voltage");
        ptr += written;
        remaining -= written;
    }
    if (this->m_watchdogFaultFlag) {
        if (ptr != reasonBuf) {
            int written = snprintf(ptr, remaining, ", ");
            ptr += written;
            remaining -= written;
        }
        snprintf(ptr, remaining, "Watchdog fault");
    }

    reasonStr = reasonBuf;
    this->log_WARNING_HI_EnteringSafeMode(reasonStr);

    // Turn off non-critical components
    this->turnOffNonCriticalComponents();

    // Set initial beacon backoff (2 seconds)
    this->updateBeaconBackoff();

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
    this->m_backoffCounter = 0;

    this->log_ACTIVITY_HI_ExitingSafeMode();

    // Turn on components (restore normal operation)
    this->turnOnComponents();

    // Restore normal beacon rate (divider = 30)
    // Note: This will be implemented when we wire to ComDelay in topology

    // Update telemetry
    this->tlmWrite_CurrentMode(static_cast<U8>(this->m_mode));

    // Notify other components of mode change
    if (this->isConnected_modeChanged_OutputPort(0)) {
        this->modeChanged_out(0);
    }

    // Save state
    this->saveState();
}

void ModeManager ::updateBeaconBackoff() {
    // Get max backoff parameter
    Fw::ParamValid valid;
    U32 maxBackoff = this->paramGet_MAX_BEACON_BACKOFF(valid);
    if (valid != Fw::ParamValid::VALID) {
        maxBackoff = 3600;  // Default: 1 hour
    }

    // Calculate exponential backoff: 2^n, capped at maxBackoff
    U32 divider = 2;
    if (this->m_backoffCounter > 0) {
        divider = 1 << this->m_backoffCounter;  // 2^m_backoffCounter
    }
    divider = std::min(divider, maxBackoff);

    // Update backoff counter for next iteration
    if (divider < maxBackoff) {
        this->m_backoffCounter++;
    }

    // Update telemetry
    this->tlmWrite_BeaconBackoffDivider(divider);
    this->log_ACTIVITY_LO_BeaconBackoffUpdated(divider);

    // Note: Actual beacon rate update will be implemented via topology connection
    // this->beaconDividerSet_out(...) when topology is wired
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

F32 ModeManager ::getCurrentVoltage() {
    // Call the voltage get port to get current system voltage
    if (this->isConnected_voltageGet_OutputPort(0)) {
        F64 voltage = this->voltageGet_out(0);
        return static_cast<F32>(voltage);  // Convert from F64 to F32
    }

    // Fallback: return last known voltage or default
    return this->m_currentVoltage > 0.0f ? this->m_currentVoltage : 8.0f;
}

}  // namespace Components
