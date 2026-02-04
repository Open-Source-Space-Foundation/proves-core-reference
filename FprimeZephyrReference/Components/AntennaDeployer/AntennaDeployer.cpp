#include "FprimeZephyrReference/Components/AntennaDeployer/AntennaDeployer.hpp"

#include <cerrno>
#include <cstring>

#include "Os/File.hpp"
#include "Os/FileSystem.hpp"
#include <zephyr/kernel.h>

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

AntennaDeployer ::~AntennaDeployer() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void AntennaDeployer ::schedIn_handler(FwIndexType portNum, U32 context) {
    (void)portNum;
    (void)context;

    switch (this->m_state) {
        case DeploymentState::IDLE:
            // Nothing to do
            break;
        case DeploymentState::BURNING:
            this->handleBurningTick();
            break;
        case DeploymentState::RETRY_WAIT:
            this->handleRetryWaitTick();
            break;
    }
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

    if (this->m_state != DeploymentState::IDLE) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
        return;
    }

    this->resetDeploymentState();
    this->m_ticksInState = 0;
    this->m_stopRequested = false;
    this->startNextAttempt();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void AntennaDeployer ::DEPLOY_STOP_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (this->m_state == DeploymentState::IDLE) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        return;
    }

    this->m_stopRequested = true;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    this->finishDeployment(Components::DeployResult::DEPLOY_RESULT_ABORT);
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

void AntennaDeployer ::startNextAttempt() {
    this->m_currentAttempt++;

    this->m_ticksInState = 0;

    this->log_ACTIVITY_HI_DeployAttempt(this->m_currentAttempt);

    this->tlmWrite_DeployAttemptCount(this->m_totalAttempts);
    this->m_totalAttempts++;

    this->m_burnTicksThisAttempt = 0;

    if (this->isConnected_burnStart_OutputPort(0)) {
        this->burnStart_out(0);
    }

    this->m_state = DeploymentState::BURNING;
}

void AntennaDeployer ::handleBurningTick() {
    this->m_ticksInState++;
    this->m_burnTicksThisAttempt = this->m_ticksInState;

    if (this->m_stopRequested) {
        this->finishDeployment(Components::DeployResult::DEPLOY_RESULT_ABORT);
        return;
    }

    if (this->m_state != DeploymentState::BURNING) {
        return;
    }

    Fw::ParamValid valid;
    const U32 burnDuration = this->paramGet_BURN_DURATION_SEC(valid);
    if (this->m_ticksInState >= burnDuration) {
        this->ensureBurnwireStopped();
        this->logBurnSignalCount();

        Fw::ParamValid attemptsValid;
        const U32 maxAttempts = this->paramGet_MAX_DEPLOY_ATTEMPTS(attemptsValid);
        if (this->m_currentAttempt >= maxAttempts) {
            this->finishDeployment(Components::DeployResult::DEPLOY_RESULT_FAILED);
            return;
        }

        this->m_state = DeploymentState::RETRY_WAIT;
        this->m_ticksInState = 0;
    }
}

void AntennaDeployer ::handleRetryWaitTick() {
    this->m_ticksInState++;

    if (this->m_stopRequested) {
        this->finishDeployment(Components::DeployResult::DEPLOY_RESULT_ABORT);
        return;
    }

    Fw::ParamValid valid;
    const U32 retryDelay = this->paramGet_RETRY_DELAY_SEC(valid);
    if (retryDelay == 0U || this->m_ticksInState >= retryDelay) {
        Fw::ParamValid attemptsValid;
        const U32 maxAttempts = this->paramGet_MAX_DEPLOY_ATTEMPTS(attemptsValid);
        if (this->m_currentAttempt >= maxAttempts) {
            this->finishDeployment(Components::DeployResult::DEPLOY_RESULT_FAILED);
            return;
        }

        this->startNextAttempt();
    }
}

void AntennaDeployer ::finishDeployment(Components::DeployResult result) {
    if (this->m_state == DeploymentState::IDLE) {
        return;
    }

    this->ensureBurnwireStopped();
    this->logBurnSignalCount();

    if (result == Components::DeployResult::DEPLOY_RESULT_SUCCESS ||
        result == Components::DeployResult::DEPLOY_RESULT_FAILED) {
        this->log_ACTIVITY_HI_DeploySuccess(this->m_currentAttempt);

        // Mark antenna as deployed by writing state file
        this->writeDeploymentState(true);
    }

    this->log_ACTIVITY_HI_DeployFinish(result, this->m_currentAttempt);

    this->resetDeploymentState();
}

void AntennaDeployer ::resetDeploymentState() {
    this->m_state = DeploymentState::IDLE;
    this->m_currentAttempt = 0;
    this->m_ticksInState = 0;
    this->m_stopRequested = false;
    this->m_burnTicksThisAttempt = 0;
}

void AntennaDeployer ::ensureBurnwireStopped() {
    if (this->isConnected_burnStop_OutputPort(0)) {
        this->burnStop_out(0);
    }
}

void AntennaDeployer ::logBurnSignalCount() {
    if (this->m_burnTicksThisAttempt > 0U) {
        this->log_ACTIVITY_LO_AntennaBurnSignalCount(this->m_burnTicksThisAttempt);
        this->m_burnTicksThisAttempt = 0;
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
