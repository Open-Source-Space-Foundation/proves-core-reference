#include "PROVESFlightControllerReference/Components/AntennaDeployer/AntennaDeployer.hpp"

#include <cerrno>
#include <cstring>

#include "Os/File.hpp"
#include "Os/FileSystem.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------

static const char* const DEPLOYED_STATE_FILE_PATH = "//antenna/antenna_deployer.bin";
static const char* const ANTENNA_DIR_PATH = "//antenna";

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

AntennaDeployer ::AntennaDeployer(const char* const compName) : AntennaDeployerComponentBase(compName) {
    // The sequencer keeps its own copy of the enum so it can build without F Prime
    static_assert(static_cast<U8>(DeploymentSequencer::Result::SUCCESS) ==
                      static_cast<U8>(Components::DeployResult::DEPLOY_RESULT_SUCCESS),
                  "DeploymentSequencer SUCCESS value must match FPP enum");
    static_assert(static_cast<U8>(DeploymentSequencer::Result::ABORT) ==
                      static_cast<U8>(Components::DeployResult::DEPLOY_RESULT_ABORT),
                  "DeploymentSequencer ABORT value must match FPP enum");
    static_assert(static_cast<U8>(DeploymentSequencer::Result::FAILED) ==
                      static_cast<U8>(Components::DeployResult::DEPLOY_RESULT_FAILED),
                  "DeploymentSequencer FAILED value must match FPP enum");
}

AntennaDeployer ::~AntennaDeployer() {}

void AntennaDeployer ::init(FwEnumStoreType instance) {
    AntennaDeployerComponentBase::init(instance);

    // Create //antenna directory if it doesn't exist
    Os::FileSystem::Status dirStatus = Os::FileSystem::createDirectory(ANTENNA_DIR_PATH, false);
    if (dirStatus != Os::FileSystem::OP_OK) {
        Fw::LogStringArg logFilePath(ANTENNA_DIR_PATH);
        Fw::LogStringArg logOperation("create_directory");
        this->log_WARNING_HI_FileOperationError(logFilePath, logOperation);
    }

    // Create deployment state file with 0 (not deployed) if it doesn't exist
    const char* path_str = DEPLOYED_STATE_FILE_PATH;

    if (!Os::FileSystem::exists(path_str)) {
        Os::File file;
        Os::File::Status status = file.open(path_str, Os::File::OPEN_WRITE);

        if (status == Os::File::OP_OK) {
            // Seek to beginning to ensure we write at the start
            FwSignedSizeType seek_offset = 0;
            (void)file.seek(seek_offset, Os::File::SeekType::ABSOLUTE);

            U8 value = 0;
            FwSizeType size = sizeof(value);
            Os::File::Status write_status = file.write(&value, size);
            (void)file.close();

            if (write_status != Os::File::OP_OK) {
                Fw::LogStringArg logFilePath(path_str);
                Fw::LogStringArg logOperation("write");
                this->log_WARNING_HI_FileOperationError(logFilePath, logOperation);
            }
        } else {
            Fw::LogStringArg logFilePath(path_str);
            Fw::LogStringArg logOperation("open_write");
            this->log_WARNING_HI_FileOperationError(logFilePath, logOperation);
        }
    }
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void AntennaDeployer ::schedIn_handler(FwIndexType portNum, U32 context) {
    (void)portNum;
    (void)context;

    if (!this->m_sequencer.isBusy()) {
        return;
    }

    Fw::ParamValid valid;
    DeploymentSequencer::Params params;
    params.burnDurationSec = this->paramGet_BURN_DURATION_SEC(valid);
    params.retryDelaySec = this->paramGet_RETRY_DELAY_SEC(valid);
    params.maxAttempts = this->paramGet_MAX_DEPLOY_ATTEMPTS(valid);
    this->applyStep(this->m_sequencer.tick(params));
}

// ----------------------------------------------------------------------
// Command handler implementations
// ----------------------------------------------------------------------

void AntennaDeployer ::DEPLOY_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Check if antenna has already been deployed
    bool isDeployed = this->readDeploymentState();
    if (isDeployed) {
        this->log_ACTIVITY_HI_DeploymentAlreadyComplete();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        return;
    }

    if (this->m_sequencer.isBusy()) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
        return;
    }

    this->applyStep(this->m_sequencer.start());
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void AntennaDeployer ::DEPLOY_STOP_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Stopping while idle does nothing, the sequencer returns no effects
    const DeploymentSequencer::Step step = this->m_sequencer.stop();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    this->applyStep(step);
}

void AntennaDeployer ::RESET_DEPLOYMENT_STATE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Write 0 to indicate not deployed
    this->writeDeploymentState(false);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void AntennaDeployer ::SET_DEPLOYMENT_STATE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool deployed) {
    // Write 1 for deployed, 0 for not deployed
    this->writeDeploymentState(deployed);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Internal helpers
// ----------------------------------------------------------------------

void AntennaDeployer ::applyStep(const DeploymentSequencer::Step& step) {
    for (std::size_t i = 0; i < step.size(); i++) {
        const DeploymentSequencer::Effect& effect = step[i];
        switch (effect.kind) {
            case DeploymentSequencer::EffectKind::BURN_START:
                if (this->isConnected_burnStart_OutputPort(0)) {
                    this->burnStart_out(0);
                }
                break;
            case DeploymentSequencer::EffectKind::BURN_STOP:
                this->ensureBurnwireStopped();
                break;
            case DeploymentSequencer::EffectKind::ATTEMPT_STARTED:
                this->log_ACTIVITY_HI_DeployAttempt(effect.attempt);
                this->tlmWrite_DeployAttemptCount(effect.attemptCount);
                break;
            case DeploymentSequencer::EffectKind::BURN_SIGNAL_COUNT:
                this->log_ACTIVITY_LO_AntennaBurnSignalCount(effect.burnTicks);
                break;
            case DeploymentSequencer::EffectKind::MARK_DEPLOYED:
                this->log_ACTIVITY_HI_DeploySuccess(effect.attempt);

                // Mark antenna as deployed by writing state file
                this->writeDeploymentState(true);
                break;
            case DeploymentSequencer::EffectKind::FINISHED:
                this->log_ACTIVITY_HI_DeployFinish(static_cast<Components::DeployResult::T>(effect.result),
                                                   effect.attempt);
                break;
        }
    }
}

void AntennaDeployer ::ensureBurnwireStopped() {
    if (this->isConnected_burnStop_OutputPort(0)) {
        this->burnStop_out(0);
    }
}

bool AntennaDeployer ::readDeploymentState() {
    const char* path_str = DEPLOYED_STATE_FILE_PATH;

    Os::File file;
    Os::File::Status status = file.open(path_str, Os::File::OPEN_READ);
    if (status != Os::File::OP_OK) {
        // File doesn't exist or can't be read, assume not deployed
        // Only log error if file exists but can't be opened (not just missing)
        if (Os::FileSystem::exists(path_str)) {
            Fw::LogStringArg logFilePath(path_str);
            Fw::LogStringArg logOperation("open_read");
            this->log_WARNING_HI_FileOperationError(logFilePath, logOperation);
        }
        return false;
    }

    U8 value = 0;
    FwSizeType size = sizeof(value);
    Os::File::Status read_status = file.read(&value, size);
    (void)file.close();

    if (read_status != Os::File::OP_OK) {
        Fw::LogStringArg logFilePath(path_str);
        Fw::LogStringArg logOperation("read");
        this->log_WARNING_HI_FileOperationError(logFilePath, logOperation);
    }

    // Return true if file contains 1, false otherwise
    return (read_status == Os::File::OP_OK && value == 1);
}

void AntennaDeployer ::writeDeploymentState(bool deployed) {
    const char* path_str = DEPLOYED_STATE_FILE_PATH;

    Os::File file;
    // Use OPEN_WRITE which creates the file if it doesn't exist
    Os::File::Status status = file.open(path_str, Os::File::OPEN_WRITE);

    if (status != Os::File::OP_OK) {
        Fw::LogStringArg logFilePath(path_str);
        Fw::LogStringArg logOperation("open_write");
        this->log_WARNING_HI_FileOperationError(logFilePath, logOperation);
        return;
    }

    // Seek to beginning to overwrite the file
    FwSignedSizeType seek_offset = 0;
    Os::File::Status seek_status = file.seek(seek_offset, Os::File::SeekType::ABSOLUTE);
    if (seek_status != Os::File::OP_OK) {
        Fw::LogStringArg logFilePath(path_str);
        Fw::LogStringArg logOperation("seek");
        this->log_WARNING_HI_FileOperationError(logFilePath, logOperation);
        (void)file.close();
        return;
    }

    U8 value = deployed ? 1 : 0;
    FwSizeType size = sizeof(value);
    Os::File::Status write_status = file.write(&value, size);
    (void)file.close();

    if (write_status != Os::File::OP_OK) {
        Fw::LogStringArg logFilePath(path_str);
        Fw::LogStringArg logOperation("write");
        this->log_WARNING_HI_FileOperationError(logFilePath, logOperation);
    }
}

}  // namespace Components
