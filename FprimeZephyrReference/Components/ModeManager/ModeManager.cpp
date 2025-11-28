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
      m_runCounter(0) {}

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
            // Validate state data before restoring (valid range: 1-3 for SAFE, NORMAL, PAYLOAD)
            if (state.mode >= static_cast<U8>(SystemMode::SAFE_MODE) &&
                state.mode <= static_cast<U8>(SystemMode::PAYLOAD_MODE)) {
                // Valid mode value - restore state
                this->m_mode = static_cast<SystemMode>(state.mode);
                this->m_safeModeEntryCount = state.safeModeEntryCount;
                this->m_payloadModeEntryCount = state.payloadModeEntryCount;

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
                // Corrupted state - use defaults
                this->m_mode = SystemMode::NORMAL;
                this->m_safeModeEntryCount = 0;
                this->m_payloadModeEntryCount = 0;
                this->turnOnComponents();
            }
        }

        file.close();
    } else {
        // File doesn't exist or can't be opened - initialize to default state
        this->m_mode = SystemMode::NORMAL;
        this->m_safeModeEntryCount = 0;
        this->m_payloadModeEntryCount = 0;
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

}  // namespace Components
