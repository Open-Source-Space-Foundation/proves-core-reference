// ======================================================================
// \title  ModeManager.cpp
// \author Auto-generated
// \brief  cpp file for ModeManager component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/ModeManager/ModeManager.hpp"

#include <algorithm>
#include <cstring>

#include "Fw/Time/Time.hpp"
#include "Fw/Types/Assert.hpp"
#include "ModeManager.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ModeManager ::ModeManager(const char* const compName)
    : ModeManagerComponentBase(compName),
      m_mode(SystemMode::NORMAL),
      m_safeModeEntryCount(0),
      m_runCounter(0),
      m_safeModeReason(Components::SafeModeReason::NONE) {
    // Compile-time verification that internal SystemMode enum matches FPP-generated enum
    static_assert(static_cast<U8>(SystemMode::SAFE_MODE) == static_cast<U8>(Components::SystemMode::SAFE_MODE),
                  "Internal SAFE_MODE value must match FPP enum");
    static_assert(static_cast<U8>(SystemMode::NORMAL) == static_cast<U8>(Components::SystemMode::NORMAL),
                  "Internal NORMAL value must match FPP enum");

    // The policy keeps its own copies of the enums so it can build without F Prime
    static_assert(static_cast<U8>(SystemMode::SAFE_MODE) == static_cast<U8>(SafeModePolicy::Mode::SAFE_MODE),
                  "SafeModePolicy SAFE_MODE value must match internal enum");
    static_assert(static_cast<U8>(SystemMode::NORMAL) == static_cast<U8>(SafeModePolicy::Mode::NORMAL),
                  "SafeModePolicy NORMAL value must match internal enum");
    static_assert(static_cast<U8>(Components::SafeModeReason::NONE) == static_cast<U8>(SafeModePolicy::Reason::NONE),
                  "SafeModePolicy NONE value must match FPP enum");
    static_assert(static_cast<U8>(Components::SafeModeReason::LOW_BATTERY) ==
                      static_cast<U8>(SafeModePolicy::Reason::LOW_BATTERY),
                  "SafeModePolicy LOW_BATTERY value must match FPP enum");
    static_assert(static_cast<U8>(Components::SafeModeReason::SYSTEM_FAULT) ==
                      static_cast<U8>(SafeModePolicy::Reason::SYSTEM_FAULT),
                  "SafeModePolicy SYSTEM_FAULT value must match FPP enum");
    static_assert(static_cast<U8>(Components::SafeModeReason::GROUND_COMMAND) ==
                      static_cast<U8>(SafeModePolicy::Reason::GROUND_COMMAND),
                  "SafeModePolicy GROUND_COMMAND value must match FPP enum");
    static_assert(static_cast<U8>(Components::SafeModeReason::EXTERNAL_REQUEST) ==
                      static_cast<U8>(SafeModePolicy::Reason::EXTERNAL_REQUEST),
                  "SafeModePolicy EXTERNAL_REQUEST value must match FPP enum");
    static_assert(static_cast<U8>(Components::SafeModeReason::LORA) == static_cast<U8>(SafeModePolicy::Reason::LORA),
                  "SafeModePolicy LORA value must match FPP enum");
    static_assert(static_cast<U8>(Components::SafeModeReason::COMMAND_LOSS) ==
                      static_cast<U8>(SafeModePolicy::Reason::COMMAND_LOSS),
                  "SafeModePolicy COMMAND_LOSS value must match FPP enum");
}

ModeManager ::~ModeManager() {}

void ModeManager ::restorePersistentState() {
    this->loadState();
}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

void ModeManager ::run_handler(FwIndexType portNum, U32 context) {
    // Increment run counter (1Hz tick counter)
    this->m_runCounter++;
    {
        Os::ScopeLock lock(m_commandLossMutex);
        this->m_policy.commandLossTick();  // keep track of seconds since last packet
    }

    // Get current voltage (used by mode-specific voltage monitoring)
    bool valid = false;
    F32 voltage = this->getCurrentVoltage(valid);

    // Get configurable parameters
    Fw::ParamValid paramValid;
    SafeModePolicy::Params params;
    params.entryVoltage = this->paramGet_SafeModeEntryVoltage(paramValid);
    params.recoveryVoltage = this->paramGet_SafeModeRecoveryVoltage(paramValid);
    params.debounceSeconds = this->paramGet_SafeModeDebounceSeconds(paramValid);

    // Voltage monitoring, see SafeModePolicy::tick for the entry and recovery rules:
    // - Entry threshold: SafeModeEntryVoltage parameter (default 6.7V)
    // - Recovery threshold: SafeModeRecoveryVoltage parameter (default 8.0V), only for LOW_BATTERY
    // - Debounce: SafeModeDebounceSeconds parameter (default 10s)
    // - SYSTEM_FAULT or GROUND_COMMAND require manual EXIT_SAFE_MODE command
    const SafeModePolicy::Action action =
        this->m_policy.tick(voltage, valid, params, static_cast<SafeModePolicy::Mode>(this->m_mode),
                            static_cast<SafeModePolicy::Reason>(static_cast<U8>(this->m_safeModeReason)));

    if (action == SafeModePolicy::Action::ENTER_LOW_BATTERY) {
        this->runSafeModeSequence();
        this->log_WARNING_HI_AutoSafeModeEntry(Components::SafeModeReason::LOW_BATTERY, valid ? voltage : 0.0f);
        this->enterSafeMode(Components::SafeModeReason::LOW_BATTERY);
    } else if (action == SafeModePolicy::Action::EXIT_AUTO) {
        this->exitSafeModeAutomatic(voltage);
    }

    // Check for command loss and trigger safe mode if timeout has expired
    commandLossCheck();

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
    const Svc::SeqArgs no_args;
    this->runSequence_out(0, safe_mode_sequence, no_args);
}

void ModeManager ::completeSequence_handler(FwIndexType portNum,
                                            FwOpcodeType opCode,
                                            U32 cmdSeq,
                                            const Fw::CmdResponse& response) {
    (void)portNum;

    if (response == Fw::CmdResponse::OK) {
        // log that sequence completed successfully
        this->log_ACTIVITY_HI_SafeModeSequenceCompleted();
    } else {
        // log that sequence failed
        this->log_WARNING_LO_SafeModeSequenceFailed(response);
    }

    // Forward completion to other listeners (e.g. StartupManager active-sequence tracking)
    if (this->isConnected_sequenceDoneNotify_OutputPort(0)) {
        this->sequenceDoneNotify_out(0, opCode, cmdSeq, response);
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

    // Mark as clean shutdown
    SafeModePolicy::StateBytes state =
        SafeModePolicy::encodeState(static_cast<SafeModePolicy::Mode>(this->m_mode), this->m_safeModeEntryCount,
                                    static_cast<SafeModePolicy::Reason>(static_cast<U8>(this->m_safeModeReason)), true);

    FwSizeType bytesToWrite = state.size();
    FwSizeType bytesWritten = bytesToWrite;
    Os::File::Status writeStatus = file.write(state.data(), bytesWritten, Os::File::WaitType::WAIT);

    // Check if write succeeded and correct number of bytes written
    if (writeStatus != Os::File::OP_OK || bytesWritten != bytesToWrite) {
        // Log failure - next boot will be misclassified as unintended reboot
        Fw::LogStringArg opStr("shutdown-write");
        this->log_WARNING_LO_StatePersistenceFailure(opStr, static_cast<I32>(writeStatus));
    }

    file.close();
}

void ModeManager ::packetRouted_handler(FwIndexType portNum) {
    Os::ScopeLock lock(m_commandLossMutex);
    this->m_policy.packetReceived();
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
        SafeModePolicy::StateBytes bytes;
        FwSizeType size = bytes.size();
        FwSizeType bytesRead = size;
        status = file.read(bytes.data(), bytesRead, Os::File::WaitType::WAIT);

        if (status == Os::File::OP_OK && bytesRead == bytes.size()) {
            const SafeModePolicy::DecodedState state = SafeModePolicy::decodeState(bytes);

            if (state.status == SafeModePolicy::DecodedState::Status::OK) {
                // Valid mode value - restore state
                this->m_mode = static_cast<SystemMode>(state.mode);
                this->m_safeModeEntryCount = state.safeModeEntryCount;
                this->m_safeModeReason = static_cast<Components::SafeModeReason::T>(state.reason);

                // An unclean shutdown while in NORMAL mode is an unintended reboot (crash, watchdog, power loss)
                unintendedReboot = state.unintendedReboot;

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
                this->log_WARNING_LO_StatePersistenceFailure(opStr, static_cast<I32>(state.rawMode));
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

    // Default to unclean - only prepareForReboot sets the clean shutdown flag
    SafeModePolicy::StateBytes state = SafeModePolicy::encodeState(
        static_cast<SafeModePolicy::Mode>(this->m_mode), this->m_safeModeEntryCount,
        static_cast<SafeModePolicy::Reason>(static_cast<U8>(this->m_safeModeReason)), false);

    FwSizeType bytesToWrite = state.size();
    FwSizeType bytesWritten = bytesToWrite;
    Os::File::Status writeStatus = file.write(state.data(), bytesWritten, Os::File::WaitType::WAIT);

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
        case Components::SafeModeReason::COMMAND_LOSS:
            reasonStr = "Loss of contact with ground";
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

void ModeManager::commandLossCheck() {
    // Protect against concurrent access to command loss state
    Os::ScopeLock lock(this->m_commandLossMutex);

    // Get command loss period from parameter
    Fw::ParamValid paramValid;
    Fw::TimeIntervalValue commLossPeriod = this->paramGet_COMM_LOSS_TIME(paramValid);
    FW_ASSERT(paramValid == Fw::ParamValid::VALID || paramValid == Fw::ParamValid::DEFAULT);

    // The policy debounces so we don't repeatedly re-trigger command loss behavior
    if (this->m_policy.commandLossCheck(commLossPeriod.get_seconds()) == SafeModePolicy::Action::ENTER_COMMAND_LOSS) {
        // Telemeter the command loss duration
        U32 commandLossDuration = this->m_policy.commandLossSeconds();
        this->log_WARNING_HI_CommandLossDetected(commandLossDuration);

        // Trigger safe mode entry due to command loss
        this->runSafeModeSequence();
        this->enterSafeMode(Components::SafeModeReason::COMMAND_LOSS);

        // Stop the watchdog to trigger a hardware power cycle as a last resort for recovery
        this->stopWatchdog_out(0);
    }
}

}  // namespace Components
