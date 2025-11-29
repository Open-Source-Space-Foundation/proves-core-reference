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
      m_payloadModeEntryCount(0),
      m_runCounter(0),
      m_lowVoltageCounter(0),
      m_safeModeReason(Components::SafeModeReason::NONE),
      m_safeModeVoltageCounter(0),
      m_recoveryVoltageCounter(0) {
    // Compile-time verification that internal SystemMode enum matches FPP-generated enum
    // This prevents silent mismatches when casting between enum types
    static_assert(static_cast<U8>(SystemMode::SAFE_MODE) == static_cast<U8>(Components::SystemMode::SAFE_MODE),
                  "Internal SAFE_MODE value must match FPP enum");
    static_assert(static_cast<U8>(SystemMode::NORMAL) == static_cast<U8>(Components::SystemMode::NORMAL),
                  "Internal NORMAL value must match FPP enum");
    static_assert(static_cast<U8>(SystemMode::PAYLOAD_MODE) == static_cast<U8>(Components::SystemMode::PAYLOAD_MODE),
                  "Internal PAYLOAD_MODE value must match FPP enum");
}

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

    // Get current voltage (used by multiple mode checks)
    bool valid = false;
    F32 voltage = this->getCurrentVoltage(valid);

    // Mode-specific voltage monitoring
    if (this->m_mode == SystemMode::PAYLOAD_MODE) {
        // Low-voltage protection for payload mode:
        // - Debounce: Requires 10 consecutive fault readings (at 1Hz) to avoid spurious triggers
        //   from transient voltage dips during load switching or sensor noise
        // - Invalid readings: Treated as faults (fail-safe) because sensor failure during payload
        //   operation could mask a real brownout condition
        // - Threshold: 7.2V chosen as minimum safe operating voltage for payload components
        bool isFault = !valid || (voltage < LOW_VOLTAGE_THRESHOLD);

        if (isFault) {
            this->m_lowVoltageCounter++;

            if (this->m_lowVoltageCounter >= LOW_VOLTAGE_DEBOUNCE_SECONDS) {
                // Trigger automatic exit from payload mode
                // Use 0.0 for voltage if reading was invalid
                this->exitPayloadModeAutomatic(valid ? voltage : 0.0f);
                this->m_lowVoltageCounter = 0;  // Reset counter
            }
        } else {
            // Voltage OK and valid - reset counter
            this->m_lowVoltageCounter = 0;
        }

        // Reset other counters when in payload mode
        this->m_safeModeVoltageCounter = 0;
        this->m_recoveryVoltageCounter = 0;

    } else if (this->m_mode == SystemMode::NORMAL) {
        // Low-voltage protection for normal mode -> safe mode entry:
        // - Threshold: 6.7V triggers safe mode entry
        // - Debounce: 10 consecutive seconds below threshold
        bool isFault = !valid || (voltage < SAFE_MODE_ENTRY_VOLTAGE);

        if (isFault) {
            this->m_safeModeVoltageCounter++;

            if (this->m_safeModeVoltageCounter >= SAFE_MODE_DEBOUNCE_SECONDS) {
                // Trigger automatic entry into safe mode
                this->log_WARNING_HI_AutoSafeModeEntry(Components::SafeModeReason::LOW_BATTERY, valid ? voltage : 0.0f);
                this->enterSafeMode(Components::SafeModeReason::LOW_BATTERY);
                this->m_safeModeVoltageCounter = 0;  // Reset counter
            }
        } else {
            // Voltage OK and valid - reset counter
            this->m_safeModeVoltageCounter = 0;
        }

        // Reset other counters when in normal mode
        this->m_lowVoltageCounter = 0;
        this->m_recoveryVoltageCounter = 0;

    } else if (this->m_mode == SystemMode::SAFE_MODE) {
        // Auto-recovery from safe mode (only if reason is LOW_BATTERY):
        // - Threshold: Voltage > 8.0V triggers auto-recovery
        // - Debounce: 10 consecutive seconds above threshold
        // - SYSTEM_FAULT or GROUND_COMMAND require manual EXIT_SAFE_MODE command
        if (this->m_safeModeReason == Components::SafeModeReason::LOW_BATTERY) {
            if (valid && voltage > SAFE_MODE_RECOVERY_VOLTAGE) {
                this->m_recoveryVoltageCounter++;

                if (this->m_recoveryVoltageCounter >= SAFE_MODE_DEBOUNCE_SECONDS) {
                    // Trigger automatic exit from safe mode
                    this->exitSafeModeAutomatic(voltage);
                    this->m_recoveryVoltageCounter = 0;  // Reset counter
                }
            } else {
                // Voltage not recovered yet - reset counter
                this->m_recoveryVoltageCounter = 0;
            }
        }
        // Note: If reason is SYSTEM_FAULT or GROUND_COMMAND, no auto-recovery - wait for manual command

        // Reset other counters when in safe mode
        this->m_lowVoltageCounter = 0;
        this->m_safeModeVoltageCounter = 0;
    }

    // Update telemetry
    this->tlmWrite_CurrentMode(static_cast<U8>(this->m_mode));
    this->tlmWrite_CurrentSafeModeReason(this->m_safeModeReason);
}

void ModeManager ::forceSafeMode_handler(FwIndexType portNum) {
    // Force entry into safe mode (called by other components)
    // Only allowed from NORMAL (sequential +1/-1 transitions)
    if (this->m_mode == SystemMode::NORMAL) {
        this->log_WARNING_HI_ExternalFaultDetected();
        this->enterSafeMode(Components::SafeModeReason::EXTERNAL_REQUEST);
    } else if (this->m_mode == SystemMode::PAYLOAD_MODE) {
        // Log that external fault was detected but we can't act on it
        // System must go PAYLOAD_MODE -> NORMAL -> SAFE_MODE sequentially
        this->log_WARNING_LO_ExternalFaultIgnoredInPayloadMode();
    }
    // Note: If already in SAFE_MODE, silently ignore (already in safest state)
}

Components::SystemMode ModeManager ::getMode_handler(FwIndexType portNum) {
    // Return the current system mode
    // Convert internal C++ enum to FPP-generated enum type
    return static_cast<Components::SystemMode::T>(this->m_mode);
}

void ModeManager ::prepareForReboot_handler(FwIndexType portNum) {
    // Called before intentional reboot to set clean shutdown flag
    // This allows us to detect unintended reboots on next startup
    this->log_ACTIVITY_HI_PreparingForReboot();

    // Save state with clean shutdown flag set
    // We directly write to file here to ensure the flag is persisted
    Os::File file;
    Os::File::Status status = file.open(STATE_FILE_PATH, Os::File::OPEN_CREATE);

    if (status != Os::File::OP_OK) {
        // Log failure - next boot will be misclassified as unintended reboot
        Fw::LogStringArg opStr("shutdown-open");
        this->log_WARNING_LO_StatePersistenceFailure(opStr, static_cast<I32>(status));
        return;
    }

    PersistentState state;
    state.mode = static_cast<U8>(this->m_mode);
    state.safeModeEntryCount = this->m_safeModeEntryCount;
    state.payloadModeEntryCount = this->m_payloadModeEntryCount;
    state.safeModeReason = static_cast<U8>(this->m_safeModeReason);
    state.cleanShutdown = 1;  // Mark as clean shutdown

    FwSizeType bytesToWrite = sizeof(PersistentState);
    FwSizeType bytesWritten = bytesToWrite;
    Os::File::Status writeStatus = file.write(reinterpret_cast<U8*>(&state), bytesWritten, Os::File::WaitType::WAIT);

    // Check if write succeeded and correct number of bytes written
    if (writeStatus != Os::File::OP_OK || bytesWritten != bytesToWrite) {
        // Log failure - next boot will be misclassified as unintended reboot
        Fw::LogStringArg opStr("shutdown-write");
        this->log_WARNING_LO_StatePersistenceFailure(opStr, static_cast<I32>(writeStatus));
    }

    file.close();
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void ModeManager ::FORCE_SAFE_MODE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Force entry into safe mode - only allowed from NORMAL (sequential +1/-1 transitions)

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
    this->enterSafeMode(Components::SafeModeReason::GROUND_COMMAND);
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
        // Reset low voltage counter to ensure consistent state (operator may be
        // re-sending command intentionally to reset any accumulated fault count)
        this->m_lowVoltageCounter = 0;
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

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

void ModeManager ::loadState() {
    Os::File file;
    Os::File::Status status = file.open(STATE_FILE_PATH, Os::File::OPEN_READ);

    bool unintendedReboot = false;

    if (status == Os::File::OP_OK) {
        PersistentState state;
        FwSizeType size = sizeof(PersistentState);
        FwSizeType bytesRead = size;
        status = file.read(reinterpret_cast<U8*>(&state), bytesRead, Os::File::WaitType::WAIT);

        if (status == Os::File::OP_OK && bytesRead == sizeof(PersistentState)) {
            // Validate state data before restoring (valid range: 1-3 for SAFE, NORMAL, PAYLOAD)
            if (state.mode >= static_cast<U8>(SystemMode::SAFE_MODE) &&
                state.mode <= static_cast<U8>(SystemMode::PAYLOAD_MODE)) {
                // Valid mode value - restore state
                this->m_mode = static_cast<SystemMode>(state.mode);
                this->m_safeModeEntryCount = state.safeModeEntryCount;
                this->m_payloadModeEntryCount = state.payloadModeEntryCount;
                this->m_safeModeReason = static_cast<Components::SafeModeReason::T>(state.safeModeReason);

                // Check for unintended reboot:
                // If cleanShutdown flag is NOT set (0) and we were in NORMAL or PAYLOAD mode,
                // this indicates an unintended reboot (crash, watchdog, power loss, etc.)
                if (state.cleanShutdown == 0 &&
                    (this->m_mode == SystemMode::NORMAL || this->m_mode == SystemMode::PAYLOAD_MODE)) {
                    unintendedReboot = true;
                }

                // Restore physical hardware state to match loaded mode
                if (this->m_mode == SystemMode::SAFE_MODE) {
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
                // Corrupted state (invalid mode value) - use defaults
                Fw::LogStringArg opStr("load-corrupt");
                this->log_WARNING_LO_StatePersistenceFailure(opStr, static_cast<I32>(state.mode));
                this->m_mode = SystemMode::NORMAL;
                this->m_safeModeEntryCount = 0;
                this->m_payloadModeEntryCount = 0;
                this->m_safeModeReason = Components::SafeModeReason::NONE;
                this->turnOnComponents();
            }
        } else {
            // Read failed or file truncated - use defaults
            Fw::LogStringArg opStr("load-read");
            this->log_WARNING_LO_StatePersistenceFailure(opStr, static_cast<I32>(status));
            this->m_mode = SystemMode::NORMAL;
            this->m_safeModeEntryCount = 0;
            this->m_payloadModeEntryCount = 0;
            this->m_safeModeReason = Components::SafeModeReason::NONE;
            this->turnOnComponents();
        }

        file.close();
    } else {
        // File doesn't exist or can't be opened - initialize to default state
        // Note: This is expected on first boot, so we use a different operation string
        Fw::LogStringArg opStr("load-open");
        this->log_WARNING_LO_StatePersistenceFailure(opStr, static_cast<I32>(status));
        this->m_mode = SystemMode::NORMAL;
        this->m_safeModeEntryCount = 0;
        this->m_payloadModeEntryCount = 0;
        this->m_safeModeReason = Components::SafeModeReason::NONE;
        this->turnOnComponents();
    }

    // Handle unintended reboot detection AFTER basic state restoration
    // This ensures we enter safe mode due to system fault
    if (unintendedReboot) {
        this->log_WARNING_HI_UnintendedRebootDetected();
        this->enterSafeMode(Components::SafeModeReason::SYSTEM_FAULT);
    }

    // Clear clean shutdown flag for next boot detection
    // This ensures that if the system crashes before the next intentional reboot,
    // we'll detect it as an unintended reboot
    this->saveState();
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
    state.safeModeReason = static_cast<U8>(this->m_safeModeReason);
    state.cleanShutdown = 0;  // Default to unclean - only prepareForReboot sets this to 1

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

void ModeManager ::enterSafeMode(Components::SafeModeReason reason) {
    // Transition to safe mode
    this->m_mode = SystemMode::SAFE_MODE;
    this->m_safeModeEntryCount++;
    this->m_safeModeReason = reason;

    // Build reason string for event log
    Fw::LogStringArg reasonStr;
    switch (reason) {
        case Components::SafeModeReason::LOW_BATTERY:
            reasonStr = "Low battery voltage";
            break;
        case Components::SafeModeReason::SYSTEM_FAULT:
            reasonStr = "System fault (unintended reboot)";
            break;
        case Components::SafeModeReason::GROUND_COMMAND:
            reasonStr = "Ground command";
            break;
        case Components::SafeModeReason::EXTERNAL_REQUEST:
            reasonStr = "External component request";
            break;
        default:
            reasonStr = "Unknown";
            break;
    }

    this->log_WARNING_HI_EnteringSafeMode(reasonStr);

    // Turn off non-critical components
    this->turnOffNonCriticalComponents();

    // Update telemetry
    this->tlmWrite_CurrentMode(static_cast<U8>(this->m_mode));
    this->tlmWrite_SafeModeEntryCount(this->m_safeModeEntryCount);
    this->tlmWrite_CurrentSafeModeReason(this->m_safeModeReason);

    // Notify other components of mode change with new mode value
    if (this->isConnected_modeChanged_OutputPort(0)) {
        Components::SystemMode fppMode = static_cast<Components::SystemMode::T>(this->m_mode);
        this->modeChanged_out(0, fppMode);
    }

    // Save state
    this->saveState();
}

void ModeManager ::exitSafeMode() {
    // Transition back to normal mode (manual command)
    this->m_mode = SystemMode::NORMAL;
    this->m_safeModeReason = Components::SafeModeReason::NONE;  // Clear reason on exit

    this->log_ACTIVITY_HI_ExitingSafeMode();

    // Turn on components (restore normal operation)
    this->turnOnComponents();

    // Update telemetry
    this->tlmWrite_CurrentMode(static_cast<U8>(this->m_mode));
    this->tlmWrite_CurrentSafeModeReason(this->m_safeModeReason);

    // Notify other components of mode change with new mode value
    if (this->isConnected_modeChanged_OutputPort(0)) {
        Components::SystemMode fppMode = static_cast<Components::SystemMode::T>(this->m_mode);
        this->modeChanged_out(0, fppMode);
    }

    // Save state
    this->saveState();
}

void ModeManager ::exitSafeModeAutomatic(F32 voltage) {
    // Automatic exit from safe mode due to voltage recovery
    // Only called when safe mode reason is LOW_BATTERY and voltage > 8.0V
    this->m_mode = SystemMode::NORMAL;
    this->m_safeModeReason = Components::SafeModeReason::NONE;  // Clear reason on exit

    this->log_ACTIVITY_HI_AutoSafeModeExit(voltage);

    // Turn on components (restore normal operation)
    this->turnOnComponents();

    // Update telemetry
    this->tlmWrite_CurrentMode(static_cast<U8>(this->m_mode));
    this->tlmWrite_CurrentSafeModeReason(this->m_safeModeReason);

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
    this->m_lowVoltageCounter = 0;  // Reset low voltage counter on mode entry

    // Build reason string
    Fw::LogStringArg reasonStr;
    char reasonBuf[REASON_STRING_SIZE];
    if (reasonOverride != nullptr) {
        reasonStr = reasonOverride;
    } else {
        snprintf(reasonBuf, sizeof(reasonBuf), "Unknown");
        reasonStr = reasonBuf;
    }

    this->log_ACTIVITY_HI_EnteringPayloadMode(reasonStr);

    // Turn on ALL load switches (faces 0-5 AND payload 6-7)
    // This ensures proper state even after automatic fault exit
    this->turnOnComponents();  // Face switches (0-5)
    this->turnOnPayload();     // Payload switches (6-7)

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
    // Transition back to normal mode (manual exit)
    this->m_mode = SystemMode::NORMAL;
    this->m_lowVoltageCounter = 0;  // Reset low voltage counter on mode exit

    this->log_ACTIVITY_HI_ExitingPayloadMode();

    // Turn off payload switches
    this->turnOffPayload();

    // Ensure face switches are ON for NORMAL mode
    // This guarantees consistent state even if faces were turned off during payload mode
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

void ModeManager ::exitPayloadModeAutomatic(F32 voltage) {
    // Automatic exit from payload mode due to fault condition (e.g., low voltage)
    // More aggressive than manual exit - turns off ALL switches
    this->m_mode = SystemMode::NORMAL;
    this->m_lowVoltageCounter = 0;  // Reset low voltage counter on mode exit

    this->log_WARNING_HI_AutoPayloadModeExit(voltage);

    // Turn OFF all load switches (aggressive - includes faces 0-5 and payload 6-7)
    this->turnOffNonCriticalComponents();

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

}  // namespace Components
