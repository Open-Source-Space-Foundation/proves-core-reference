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
      m_runCounter(0),
      m_safeModeReason(Components::SafeModeReason::NONE),
      m_safeModeVoltageCounter(0),
      m_recoveryVoltageCounter(0) {
    // Compile-time verification that internal SystemMode enum matches FPP-generated enum
    static_assert(static_cast<U8>(SystemMode::SAFE_MODE) == static_cast<U8>(Components::SystemMode::SAFE_MODE),
                  "Internal SAFE_MODE value must match FPP enum");
    static_assert(static_cast<U8>(SystemMode::NORMAL) == static_cast<U8>(Components::SystemMode::NORMAL),
                  "Internal NORMAL value must match FPP enum");
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

    // Get current voltage (used by mode-specific voltage monitoring)
    bool valid = false;
    F32 voltage = this->getCurrentVoltage(valid);

    // Get configurable parameters
    Fw::ParamValid paramValid;
    F32 entryVoltage = this->paramGet_SafeModeEntryVoltage(paramValid);
    F32 recoveryVoltage = this->paramGet_SafeModeRecoveryVoltage(paramValid);
    U32 debounceSeconds = this->paramGet_SafeModeDebounceSeconds(paramValid);

    // Mode-specific voltage monitoring
    if (this->m_mode == SystemMode::NORMAL) {
        // Low-voltage protection for normal mode -> safe mode entry:
        // - Threshold: configurable via SafeModeEntryVoltage parameter (default 6.7V)
        // - Debounce: configurable via SafeModeDebounceSeconds parameter (default 10s)
        bool isFault = !valid || (voltage < entryVoltage);

        if (isFault) {
            this->m_safeModeVoltageCounter++;

            if (this->m_safeModeVoltageCounter >= debounceSeconds) {
                // Trigger automatic entry into safe mode
                this->runSafeModeSequence();
                this->log_WARNING_HI_AutoSafeModeEntry(Components::SafeModeReason::LOW_BATTERY, valid ? voltage : 0.0f);
                this->enterSafeMode(Components::SafeModeReason::LOW_BATTERY);
                this->m_safeModeVoltageCounter = 0;  // Reset counter
            }
        } else {
            // Voltage OK and valid - reset counter
            this->m_safeModeVoltageCounter = 0;
        }

        // Reset recovery counter when in normal mode
        this->m_recoveryVoltageCounter = 0;

    } else if (this->m_mode == SystemMode::SAFE_MODE) {
        // Auto-recovery from safe mode (only if reason is LOW_BATTERY):
        // - Threshold: configurable via SafeModeRecoveryVoltage parameter (default 8.0V)
        // - Debounce: configurable via SafeModeDebounceSeconds parameter (default 10s)
        // - SYSTEM_FAULT or GROUND_COMMAND require manual EXIT_SAFE_MODE command
        if (this->m_safeModeReason == Components::SafeModeReason::LOW_BATTERY) {
            if (valid && voltage > recoveryVoltage) {
                this->m_recoveryVoltageCounter++;

                if (this->m_recoveryVoltageCounter >= debounceSeconds) {
                    // Trigger automatic exit from safe mode
                    this->exitSafeModeAutomatic(voltage);
                    this->m_recoveryVoltageCounter = 0;  // Reset counter
                }
            } else {
                // Voltage not recovered yet - reset counter
                this->m_recoveryVoltageCounter = 0;
            }
        }
        // Note: If reason is SYSTEM_FAULT, GROUND_COMMAND, or EXTERNAL_REQUEST, no auto-recovery

        // Reset safe mode entry counter when in safe mode
        this->m_safeModeVoltageCounter = 0;
    }

    // Update telemetry
    this->tlmWrite_CurrentMode(static_cast<U8>(this->m_mode));
    this->tlmWrite_CurrentSafeModeReason(this->m_safeModeReason);
    this->tlmWrite_SafeModeEntryCount(this->m_safeModeEntryCount);
}

void ModeManager ::forceSafeMode_handler(FwIndexType portNum, const Components::SafeModeReason& reason) {
    // Force entry into safe mode (called by other components)
    // Only allowed from NORMAL (sequential +1/-1 transitions)
    if (this->m_mode == SystemMode::NORMAL) {
        this->log_WARNING_HI_ExternalFaultDetected();

        // Use provided reason, defaulting to EXTERNAL_REQUEST if NONE is passed
        Components::SafeModeReason effectiveReason = reason;
        if (reason == Components::SafeModeReason::NONE) {
            effectiveReason = Components::SafeModeReason::EXTERNAL_REQUEST;
        }

        this->runSafeModeSequence();

        this->enterSafeMode(effectiveReason);
    } else if (this->m_mode == SystemMode::SAFE_MODE) {
        this->log_WARNING_LO_SafeModeRequestIgnored();
    }
    // Note: Request ignored if already in SAFE_MODE
}

void ModeManager ::runSafeModeSequence() {
    // run the safe mode sequence
    Fw::ParamValid is_valid;
    Fw::ParamString safe_mode_sequence = this->paramGet_SAFEMODE_SEQUENCE_FILE(is_valid);
    FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);
    this->runSequence_out(0, safe_mode_sequence);
}

void ModeManager ::completeSequence_handler(FwIndexType portNum,
                                            FwOpcodeType opCode,
                                            U32 cmdSeq,
                                            const Fw::CmdResponse& response) {
    (void)portNum;
    (void)opCode;
    (void)cmdSeq;

    if (response == Fw::CmdResponse::OK) {
        // log that sequence completed successfully
        this->log_ACTIVITY_HI_SafeModeSequenceCompleted();
    } else {
        // log that sequence failed
        this->log_WARNING_LO_SafeModeSequenceFailed(response);
    }
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
    Os::File::Status status = file.open(STATE_FILE_PATH, Os::File::OPEN_CREATE, Os::File::OVERWRITE);

    if (status != Os::File::OP_OK) {
        // Log failure - next boot will be misclassified as unintended reboot
        Fw::LogStringArg opStr("shutdown-open");
        this->log_WARNING_LO_StatePersistenceFailure(opStr, static_cast<I32>(status));
        return;
    }

    PersistentState state;
    state.mode = static_cast<U8>(this->m_mode);
    state.safeModeEntryCount = this->m_safeModeEntryCount;
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
    // Force entry into safe mode

    // Already in safe mode - idempotent success
    if (this->m_mode == SystemMode::SAFE_MODE) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        return;
    }

    this->runSafeModeSequence();

    // Enter safe mode from NORMAL
    this->log_ACTIVITY_HI_ManualSafeModeEntry();
    this->enterSafeMode(Components::SafeModeReason::GROUND_COMMAND);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void ModeManager ::EXIT_SAFE_MODE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Manual command to exit safe mode
    this->exitSafeMode();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void ModeManager ::GET_CURRENT_MODE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Components::SystemMode fppMode = static_cast<Components::SystemMode::T>(this->m_mode);
    this->log_ACTIVITY_LO_CurrentModeReading(fppMode);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void ModeManager ::GET_SAFE_MODE_REASON_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->log_ACTIVITY_LO_CurrentSafeModeReasonReading(this->m_safeModeReason);
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
            // Validate state data before restoring (valid range: 1-2 for SAFE, NORMAL)
            if (state.mode >= static_cast<U8>(SystemMode::SAFE_MODE) &&
                state.mode <= static_cast<U8>(SystemMode::NORMAL)) {
                // Valid mode value - restore state
                this->m_mode = static_cast<SystemMode>(state.mode);
                this->m_safeModeEntryCount = state.safeModeEntryCount;
                this->m_safeModeReason = static_cast<Components::SafeModeReason::T>(state.safeModeReason);

                // Check for unintended reboot:
                // If cleanShutdown flag is NOT set (0) and we were in NORMAL mode,
                // this indicates an unintended reboot (crash, watchdog, power loss, etc.)
                if (state.cleanShutdown == 0 && this->m_mode == SystemMode::NORMAL) {
                    unintendedReboot = true;
                }

                // Restore physical hardware state to match loaded mode
                if (this->m_mode == SystemMode::SAFE_MODE) {
                    // Turn off non-critical components to match safe mode state
                    this->turnOffNonCriticalComponents();

                    // TODO: commented out because this crashes the board on boot
                    // run radio safe to match default safe params
                    // this->runSafeModeSequence();

                    // Log that we're restoring safe mode (not entering it fresh)
                    Fw::LogStringArg reasonStr("State restored from persistent storage");
                    this->log_WARNING_HI_EnteringSafeMode(reasonStr);
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
                this->m_safeModeReason = Components::SafeModeReason::NONE;
                this->turnOnComponents();
            }
        } else {
            // Read failed or file truncated - use defaults
            Fw::LogStringArg opStr("load-read");
            this->log_WARNING_LO_StatePersistenceFailure(opStr, static_cast<I32>(status));
            this->m_mode = SystemMode::NORMAL;
            this->m_safeModeEntryCount = 0;
            this->m_safeModeReason = Components::SafeModeReason::NONE;
            this->turnOnComponents();
        }

        file.close();
    } else {
        // Initialize to default state
        this->m_mode = SystemMode::NORMAL;
        this->m_safeModeEntryCount = 0;
        this->m_safeModeReason = Components::SafeModeReason::NONE;
        this->turnOnComponents();

        // Only log warning for unexpected errors, not for expected "file not found" on first boot
        if (status != Os::File::DOESNT_EXIST) {
            Fw::LogStringArg opStr("load-open");
            this->log_WARNING_LO_StatePersistenceFailure(opStr, static_cast<I32>(status));
        }
        // Note: DOESNT_EXIST is expected on first boot - no warning needed
    }

    // Handle unintended reboot detection AFTER basic state restoration
    // This ensures we enter safe mode due to system fault
    if (unintendedReboot) {
        // On unintended reboot, enter safe mode and run the safe mode sequence
        // (e.g., to reset radio parameters and enforce any transmit delay policy)
        this->log_WARNING_HI_UnintendedRebootDetected();
        this->enterSafeMode(Components::SafeModeReason::SYSTEM_FAULT);

        // TODO: commented out because this crashes the board on boot if ran
        // this->runSafeModeSequence();
    }

    // Clear clean shutdown flag for next boot detection
    // This ensures that if the system crashes before the next intentional reboot,
    // we'll detect it as an unintended reboot
    this->saveState();
}

void ModeManager ::saveState() {
    Os::File file;
    Os::File::Status status = file.open(STATE_FILE_PATH, Os::File::OPEN_CREATE, Os::File::OVERWRITE);

    if (status != Os::File::OP_OK) {
        // Log failure to open file, but allow component to continue
        Fw::LogStringArg opStr("save-open");
        this->log_WARNING_LO_StatePersistenceFailure(opStr, static_cast<I32>(status));
        return;
    }

    PersistentState state;
    state.mode = static_cast<U8>(this->m_mode);
    state.safeModeEntryCount = this->m_safeModeEntryCount;
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
        case Components::SafeModeReason::LORA:
            reasonStr = "LoRa communication fault";
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
    Components::SystemMode fppMode = static_cast<Components::SystemMode::T>(this->m_mode);
    for (FwIndexType i = 0; i < this->getNum_modeChanged_OutputPorts(); i++) {
        if (!this->isConnected_modeChanged_OutputPort(i)) {
            continue;
        }
        this->modeChanged_out(i, fppMode);
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

void ModeManager ::turnOffNonCriticalComponents() {
    for (FwIndexType i = 0; i < this->getNum_loadSwitchTurnOff_OutputPorts(); i++) {
        if (!this->isConnected_loadSwitchTurnOff_OutputPort(i)) {
            continue;
        }
        this->loadSwitchTurnOff_out(i);
    }
}

void ModeManager ::turnOnComponents() {
    for (FwIndexType i = 0; i < this->getNum_loadSwitchTurnOn_OutputPorts(); i++) {
        if (!this->isConnected_loadSwitchTurnOn_OutputPort(i)) {
            continue;
        }
        this->loadSwitchTurnOn_out(i);
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
