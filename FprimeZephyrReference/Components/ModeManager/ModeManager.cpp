// ======================================================================
// \title  ModeManager.cpp
// \author Auto-generated
// \brief  cpp file for ModeManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/ModeManager/ModeManager.hpp"

#include <algorithm>
#include <cstring>

#include "FprimeZephyrReference/Components/ModeManager/PicoSleep.hpp"
#include "Fw/Types/Assert.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ModeManager ::ModeManager(const char* const compName)
    : ModeManagerComponentBase(compName),
      m_mode(SystemMode::NORMAL),
      m_safeModeEntryCount(0),
      m_payloadModeEntryCount(0),
      m_runCounter(0),
      m_inHibernationWakeWindow(false),
      m_wakeWindowCounter(0),
      m_hibernationCycleCount(0),
      m_hibernationTotalSeconds(0),
      m_sleepDurationSec(3600),  // Default 60 minutes
      m_wakeDurationSec(60) {}   // Default 1 minute

ModeManager ::~ModeManager() {}

void ModeManager ::init(FwSizeType queueDepth, FwEnumStoreType instance) {
    ModeManagerComponentBase::init(queueDepth, instance);
    this->loadState();
}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

void ModeManager ::run_handler(FwIndexType portNum, U32 context) {
    // Increment run counter (1Hz tick counter)
    this->m_runCounter++;

    // Handle hibernation wake window timing
    if (this->m_mode == SystemMode::HIBERNATION_MODE && this->m_inHibernationWakeWindow) {
        this->handleWakeWindowTick();
    }

    // Update telemetry
    this->tlmWrite_CurrentMode(static_cast<U8>(this->m_mode));
}

void ModeManager ::forceSafeMode_handler(FwIndexType portNum) {
    // Force entry into safe mode (called by other components)
    // Only allowed from NORMAL (sequential +1/-1 transitions)
    if (this->m_mode == SystemMode::NORMAL) {
        this->log_WARNING_HI_ExternalFaultDetected();
        this->enterSafeMode("External component request");
    }
    // Note: Request ignored if in PAYLOAD_MODE or already in SAFE_MODE
}

Components::SystemMode ModeManager ::getMode_handler(FwIndexType portNum) {
    // Return the current system mode
    // Convert internal C++ enum to FPP-generated enum type
    return static_cast<Components::SystemMode::T>(this->m_mode);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void ModeManager ::FORCE_SAFE_MODE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Force entry into safe mode - only allowed from NORMAL (sequential +1/-1 transitions)

    // Reject if in hibernation mode - must use EXIT_HIBERNATION instead
    // This ensures proper cleanup of hibernation state (m_inHibernationWakeWindow, counters)
    if (this->m_mode == SystemMode::HIBERNATION_MODE) {
        Fw::LogStringArg cmdNameStr("FORCE_SAFE_MODE");
        Fw::LogStringArg reasonStr("Use EXIT_HIBERNATION to exit hibernation mode");
        this->log_WARNING_LO_CommandValidationFailed(cmdNameStr, reasonStr);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    // Reject if in payload mode - must exit payload mode first
    if (this->m_mode == SystemMode::PAYLOAD_MODE) {
        Fw::LogStringArg cmdNameStr("FORCE_SAFE_MODE");
        Fw::LogStringArg reasonStr("Must exit payload mode first");
        this->log_WARNING_LO_CommandValidationFailed(cmdNameStr, reasonStr);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    // Already in safe mode - idempotent success
    if (this->m_mode == SystemMode::SAFE_MODE) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        return;
    }

    // Enter safe mode from NORMAL
    this->log_ACTIVITY_HI_ManualSafeModeEntry();
    this->enterSafeMode("Ground command");
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

void ModeManager ::ENTER_PAYLOAD_MODE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Command to enter payload mode - only allowed from NORMAL mode

    // Check if currently in safe mode (not allowed)
    if (this->m_mode == SystemMode::SAFE_MODE) {
        Fw::LogStringArg cmdNameStr("ENTER_PAYLOAD_MODE");
        Fw::LogStringArg reasonStr("Cannot enter payload mode from safe mode");
        this->log_WARNING_LO_CommandValidationFailed(cmdNameStr, reasonStr);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    // Check if already in payload mode
    if (this->m_mode == SystemMode::PAYLOAD_MODE) {
        // Already in payload mode - success (idempotent)
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        return;
    }

    // Enter payload mode
    this->log_ACTIVITY_HI_ManualPayloadModeEntry();
    this->enterPayloadMode("Ground command");
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void ModeManager ::EXIT_PAYLOAD_MODE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Command to exit payload mode

    // Check if currently in payload mode
    if (this->m_mode != SystemMode::PAYLOAD_MODE) {
        Fw::LogStringArg cmdNameStr("EXIT_PAYLOAD_MODE");
        Fw::LogStringArg reasonStr("Not currently in payload mode");
        this->log_WARNING_LO_CommandValidationFailed(cmdNameStr, reasonStr);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    // Exit payload mode
    this->exitPayloadMode();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void ModeManager ::ENTER_HIBERNATION_cmdHandler(FwOpcodeType opCode,
                                                U32 cmdSeq,
                                                U32 sleepDurationSec,
                                                U32 wakeDurationSec) {
    // Command to enter hibernation mode - only allowed from SAFE_MODE

    // Validate: must be in SAFE_MODE
    if (this->m_mode != SystemMode::SAFE_MODE) {
        Fw::LogStringArg cmdNameStr("ENTER_HIBERNATION");
        Fw::LogStringArg reasonStr("Can only enter hibernation from safe mode");
        this->log_WARNING_LO_CommandValidationFailed(cmdNameStr, reasonStr);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    // Use defaults if zero passed
    U32 sleepSec = (sleepDurationSec == 0) ? 3600 : sleepDurationSec;  // Default 60 min
    U32 wakeSec = (wakeDurationSec == 0) ? 60 : wakeDurationSec;       // Default 1 min

    // Send command response BEFORE entering hibernation
    // enterHibernation -> enterDormantSleep -> sys_reboot() does not return on success
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);

    // Enter hibernation mode (does not return on success - system reboots)
    this->enterHibernation(sleepSec, wakeSec, "Ground command");
}

void ModeManager ::EXIT_HIBERNATION_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Command to exit hibernation mode

    // Validate: must be in HIBERNATION_MODE and in wake window
    if (this->m_mode != SystemMode::HIBERNATION_MODE) {
        Fw::LogStringArg cmdNameStr("EXIT_HIBERNATION");
        Fw::LogStringArg reasonStr("Not currently in hibernation mode");
        this->log_WARNING_LO_CommandValidationFailed(cmdNameStr, reasonStr);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    if (!this->m_inHibernationWakeWindow) {
        Fw::LogStringArg cmdNameStr("EXIT_HIBERNATION");
        Fw::LogStringArg reasonStr("Not in wake window");
        this->log_WARNING_LO_CommandValidationFailed(cmdNameStr, reasonStr);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    // Exit hibernation mode
    this->exitHibernation();
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
        FwSizeType bytesRead = size;
        status = file.read(reinterpret_cast<U8*>(&state), bytesRead, Os::File::WaitType::WAIT);

        if (status == Os::File::OP_OK && bytesRead == sizeof(PersistentState)) {
            // Validate state data before restoring (valid range: 0-3 for HIBERNATION, SAFE, NORMAL, PAYLOAD)
            if (state.mode <= static_cast<U8>(SystemMode::PAYLOAD_MODE)) {
                // Valid mode value - restore state
                this->m_mode = static_cast<SystemMode>(state.mode);
                this->m_safeModeEntryCount = state.safeModeEntryCount;
                this->m_payloadModeEntryCount = state.payloadModeEntryCount;
                this->m_hibernationCycleCount = state.hibernationCycleCount;
                this->m_hibernationTotalSeconds = state.hibernationTotalSeconds;
                this->m_sleepDurationSec = state.sleepDurationSec;
                this->m_wakeDurationSec = state.wakeDurationSec;

                // Restore physical hardware state to match loaded mode
                if (this->m_mode == SystemMode::HIBERNATION_MODE) {
                    // Woke from dormant sleep - enter wake window
                    // Keep all load switches OFF - we're in minimal power mode
                    this->turnOffNonCriticalComponents();

                    // Start wake window (radio is already initializing in Main.cpp)
                    this->startWakeWindow();
                } else if (this->m_mode == SystemMode::SAFE_MODE) {
                    // Turn off non-critical components to match safe mode state
                    this->turnOffNonCriticalComponents();

                    // Log that we're restoring safe mode (not entering it fresh)
                    Fw::LogStringArg reasonStr("State restored from persistent storage");
                    this->log_WARNING_HI_EnteringSafeMode(reasonStr);
                } else if (this->m_mode == SystemMode::PAYLOAD_MODE) {
                    // PAYLOAD_MODE - turn on face switches AND payload switches
                    this->turnOnComponents();  // Face switches (0-5)
                    this->turnOnPayload();     // Payload switches (6-7)

                    // Log that we're restoring payload mode
                    Fw::LogStringArg reasonStr("State restored from persistent storage");
                    this->log_ACTIVITY_HI_EnteringPayloadMode(reasonStr);
                } else {
                    // NORMAL mode - ensure components are turned on
                    this->turnOnComponents();
                }
            } else {
                // Corrupted state - use defaults
                this->m_mode = SystemMode::NORMAL;
                this->m_safeModeEntryCount = 0;
                this->m_payloadModeEntryCount = 0;
                this->m_hibernationCycleCount = 0;
                this->m_hibernationTotalSeconds = 0;
                this->m_sleepDurationSec = 3600;
                this->m_wakeDurationSec = 60;
                this->turnOnComponents();
            }
        } else {
            // Read failed or size mismatch (possibly old struct version)
            // Initialize to safe defaults and configure hardware
            this->m_mode = SystemMode::NORMAL;
            this->m_safeModeEntryCount = 0;
            this->m_payloadModeEntryCount = 0;
            this->m_hibernationCycleCount = 0;
            this->m_hibernationTotalSeconds = 0;
            this->m_sleepDurationSec = 3600;
            this->m_wakeDurationSec = 60;
            this->turnOnComponents();
        }

        file.close();
    } else {
        // File doesn't exist or can't be opened - initialize to default state
        this->m_mode = SystemMode::NORMAL;
        this->m_safeModeEntryCount = 0;
        this->m_payloadModeEntryCount = 0;
        this->m_hibernationCycleCount = 0;
        this->m_hibernationTotalSeconds = 0;
        this->m_sleepDurationSec = 3600;
        this->m_wakeDurationSec = 60;
        this->turnOnComponents();
    }
}

void ModeManager ::saveState() {
    Os::File file;
    Os::File::Status status = file.open(STATE_FILE_PATH, Os::File::OPEN_CREATE);

    if (status != Os::File::OP_OK) {
        // Log failure to open file, but allow component to continue
        Fw::LogStringArg opStr("save-open");
        this->log_WARNING_LO_StatePersistenceFailure(opStr, static_cast<I32>(status));
        return;
    }

    PersistentState state;
    state.mode = static_cast<U8>(this->m_mode);
    state.safeModeEntryCount = this->m_safeModeEntryCount;
    state.payloadModeEntryCount = this->m_payloadModeEntryCount;
    state.hibernationCycleCount = this->m_hibernationCycleCount;
    state.hibernationTotalSeconds = this->m_hibernationTotalSeconds;
    state.sleepDurationSec = this->m_sleepDurationSec;
    state.wakeDurationSec = this->m_wakeDurationSec;

    FwSizeType bytesToWrite = sizeof(PersistentState);
    FwSizeType bytesWritten = bytesToWrite;
    Os::File::Status writeStatus = file.write(reinterpret_cast<U8*>(&state), bytesWritten, Os::File::WaitType::WAIT);

    // Check if write succeeded and correct number of bytes written
    if (writeStatus != Os::File::OP_OK || bytesWritten != bytesToWrite) {
        // Log failure but allow component to continue operating
        // This is critical - we don't want to crash during safe mode entry
        Fw::LogStringArg opStr("save-write");
        this->log_WARNING_LO_StatePersistenceFailure(opStr, static_cast<I32>(writeStatus));
    }

    file.close();
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

    // Notify other components of mode change with new mode value
    if (this->isConnected_modeChanged_OutputPort(0)) {
        Components::SystemMode fppMode = static_cast<Components::SystemMode::T>(this->m_mode);
        this->modeChanged_out(0, fppMode);
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

    // Notify other components of mode change with new mode value
    if (this->isConnected_modeChanged_OutputPort(0)) {
        Components::SystemMode fppMode = static_cast<Components::SystemMode::T>(this->m_mode);
        this->modeChanged_out(0, fppMode);
    }

    // Save state
    this->saveState();
}

void ModeManager ::enterPayloadMode(const char* reasonOverride) {
    // Transition to payload mode
    this->m_mode = SystemMode::PAYLOAD_MODE;
    this->m_payloadModeEntryCount++;

    // Build reason string
    Fw::LogStringArg reasonStr;
    char reasonBuf[100];
    if (reasonOverride != nullptr) {
        reasonStr = reasonOverride;
    } else {
        snprintf(reasonBuf, sizeof(reasonBuf), "Unknown");
        reasonStr = reasonBuf;
    }

    this->log_ACTIVITY_HI_EnteringPayloadMode(reasonStr);

    // Turn on payload switches (6 & 7)
    this->turnOnPayload();

    // Update telemetry
    this->tlmWrite_CurrentMode(static_cast<U8>(this->m_mode));
    this->tlmWrite_PayloadModeEntryCount(this->m_payloadModeEntryCount);

    // Notify other components of mode change with new mode value
    if (this->isConnected_modeChanged_OutputPort(0)) {
        Components::SystemMode fppMode = static_cast<Components::SystemMode::T>(this->m_mode);
        this->modeChanged_out(0, fppMode);
    }

    // Save state
    this->saveState();
}

void ModeManager ::exitPayloadMode() {
    // Transition back to normal mode
    this->m_mode = SystemMode::NORMAL;

    this->log_ACTIVITY_HI_ExitingPayloadMode();

    // Turn off payload switches
    this->turnOffPayload();

    // Ensure face switches (0-5) are ON for NORMAL mode
    // This guarantees consistent state regardless of transition path
    this->turnOnComponents();

    // Update telemetry
    this->tlmWrite_CurrentMode(static_cast<U8>(this->m_mode));

    // Notify other components of mode change with new mode value
    if (this->isConnected_modeChanged_OutputPort(0)) {
        Components::SystemMode fppMode = static_cast<Components::SystemMode::T>(this->m_mode);
        this->modeChanged_out(0, fppMode);
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
    // Turn ON face load switches (0-5) to restore normal operation
    // Note: Payload switches (6-7) are NOT turned on here - they require PAYLOAD_MODE

    // Send turn on signal to face load switches only (indices 0-5)
    for (FwIndexType i = 0; i < 6; i++) {
        if (this->isConnected_loadSwitchTurnOn_OutputPort(i)) {
            this->loadSwitchTurnOn_out(i);
        }
    }
}

void ModeManager ::turnOnPayload() {
    // Turn ON payload load switches (6 = payload power, 7 = payload battery)
    if (this->isConnected_loadSwitchTurnOn_OutputPort(6)) {
        this->loadSwitchTurnOn_out(6);
    }
    if (this->isConnected_loadSwitchTurnOn_OutputPort(7)) {
        this->loadSwitchTurnOn_out(7);
    }
}

void ModeManager ::turnOffPayload() {
    // Turn OFF payload load switches (6 = payload power, 7 = payload battery)
    if (this->isConnected_loadSwitchTurnOff_OutputPort(6)) {
        this->loadSwitchTurnOff_out(6);
    }
    if (this->isConnected_loadSwitchTurnOff_OutputPort(7)) {
        this->loadSwitchTurnOff_out(7);
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

void ModeManager ::enterHibernation(U32 sleepDurationSec, U32 wakeDurationSec, const char* reason) {
    // Transition to hibernation mode
    this->m_mode = SystemMode::HIBERNATION_MODE;
    this->m_inHibernationWakeWindow = false;
    this->m_sleepDurationSec = sleepDurationSec;
    this->m_wakeDurationSec = wakeDurationSec;

    // Build reason string
    Fw::LogStringArg reasonStr;
    if (reason != nullptr) {
        reasonStr = reason;
    } else {
        reasonStr = "Unknown";
    }

    // Log entering hibernation with parameters
    this->log_WARNING_HI_EnteringHibernation(reasonStr, sleepDurationSec, wakeDurationSec);

    // Turn off ALL load switches (0-7) - minimal power mode
    this->turnOffNonCriticalComponents();

    // Update telemetry
    this->tlmWrite_CurrentMode(static_cast<U8>(this->m_mode));
    this->tlmWrite_HibernationCycleCount(this->m_hibernationCycleCount);
    this->tlmWrite_HibernationTotalSeconds(this->m_hibernationTotalSeconds);

    // Notify other components of mode change
    if (this->isConnected_modeChanged_OutputPort(0)) {
        Components::SystemMode fppMode = static_cast<Components::SystemMode::T>(this->m_mode);
        this->modeChanged_out(0, fppMode);
    }

    // Save state before dormant sleep (includes sleep/wake durations for resume)
    this->saveState();

    // Enter dormant sleep - this does NOT return on success
    this->enterDormantSleep();
}

void ModeManager ::exitHibernation() {
    // Transition from hibernation to safe mode
    this->m_mode = SystemMode::SAFE_MODE;
    this->m_inHibernationWakeWindow = false;

    // Log exit with statistics
    this->log_ACTIVITY_HI_ExitingHibernation(this->m_hibernationCycleCount, this->m_hibernationTotalSeconds);

    // Update telemetry
    this->tlmWrite_CurrentMode(static_cast<U8>(this->m_mode));

    // Notify other components of mode change
    if (this->isConnected_modeChanged_OutputPort(0)) {
        Components::SystemMode fppMode = static_cast<Components::SystemMode::T>(this->m_mode);
        this->modeChanged_out(0, fppMode);
    }

    // Save state (now in SAFE_MODE)
    this->saveState();
}

void ModeManager ::enterDormantSleep() {
    // Increment counters BEFORE sleep (in case of unexpected wake)
    this->m_hibernationCycleCount++;
    this->m_hibernationTotalSeconds += this->m_sleepDurationSec;

    // Log that we're starting a sleep cycle
    this->log_ACTIVITY_LO_HibernationSleepCycle(this->m_hibernationCycleCount);

    // Update telemetry before sleep
    this->tlmWrite_HibernationCycleCount(this->m_hibernationCycleCount);
    this->tlmWrite_HibernationTotalSeconds(this->m_hibernationTotalSeconds);

    // Save updated counters
    this->saveState();

    // Use Pico SDK to enter dormant mode with AON timer alarm
    // On RP2350:
    //   - Returns true: Successfully woke from AON timer alarm
    //   - Returns false: Dormant mode entry failed (native/sim or hardware error)
    //   - Does not return: sys_reboot fallback was used (loadState handles wake)
    bool success = PicoSleep::sleepForSeconds(this->m_sleepDurationSec);

    if (success) {
        // Successfully woke from AON timer dormant mode!
        // Start the wake window - same behavior as reboot-based wake
        this->startWakeWindow();
    } else {
        // Dormant mode entry failed
        // Roll back counters since sleep didn't actually occur
        this->m_hibernationCycleCount--;
        this->m_hibernationTotalSeconds -= this->m_sleepDurationSec;

        // Update telemetry with corrected values
        this->tlmWrite_HibernationCycleCount(this->m_hibernationCycleCount);
        this->tlmWrite_HibernationTotalSeconds(this->m_hibernationTotalSeconds);

        // Log failure with HIGH severity - ground already saw OK response!
        // This is the only way ground knows the command actually failed
        Fw::LogStringArg reasonStr("Dormant mode entry failed - hardware or AON timer error");
        this->log_WARNING_HI_HibernationEntryFailed(reasonStr);

        // Exit hibernation mode since we couldn't enter dormant
        // (this will save state with corrected counters)
        this->exitHibernation();
    }
}

void ModeManager ::startWakeWindow() {
    this->m_inHibernationWakeWindow = true;
    this->m_wakeWindowCounter = 0;

    // Log that we're in a wake window
    this->log_ACTIVITY_LO_HibernationWakeWindow(this->m_hibernationCycleCount);

    // Update telemetry
    this->tlmWrite_HibernationCycleCount(this->m_hibernationCycleCount);
    this->tlmWrite_HibernationTotalSeconds(this->m_hibernationTotalSeconds);
}

void ModeManager ::handleWakeWindowTick() {
    // Called from run_handler at 1Hz when in hibernation wake window
    this->m_wakeWindowCounter++;

    if (this->m_wakeWindowCounter >= this->m_wakeDurationSec) {
        // Wake window elapsed, no EXIT_HIBERNATION received
        // Go back to dormant sleep
        this->m_inHibernationWakeWindow = false;
        this->enterDormantSleep();
    }
    // Otherwise, continue listening for EXIT_HIBERNATION command
}

}  // namespace Components
