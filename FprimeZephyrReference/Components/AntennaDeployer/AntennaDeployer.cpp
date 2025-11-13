#include "FprimeZephyrReference/Components/AntennaDeployer/AntennaDeployer.hpp"

#include "Os/File.hpp"
#include "Os/FileSystem.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

AntennaDeployer ::AntennaDeployer(const char* const compName) : AntennaDeployerComponentBase(compName) {}

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
        case DeploymentState::QUIET_WAIT:
            this->handleQuietWaitTick();
            break;
        case DeploymentState::BURNING:
            this->handleBurningTick();
            break;
        case DeploymentState::RETRY_WAIT:
            this->handleRetryWaitTick();
            break;
    }
}

void AntennaDeployer ::distanceIn_handler(FwIndexType portNum, F32 distance, bool valid) {
    (void)portNum;

    this->m_lastDistance = distance;
    this->m_lastDistanceValid = valid && this->isDistanceWithinValidRange(distance);

    if (!this->m_lastDistanceValid) {
        this->log_WARNING_LO_InvalidDistanceMeasurement(distance);
        return;
    }

    this->tlmWrite_LastDistance(distance);

    if (this->m_state == DeploymentState::IDLE) {
        return;
    }

    if (this->isDistanceDeployed(distance)) {
        this->finishDeployment(Components::DeployResult::DEPLOY_RESULT_SUCCESS);
    }
}

// ----------------------------------------------------------------------
// Command handler implementations
// ----------------------------------------------------------------------

void AntennaDeployer ::DEPLOY_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Check if antenna has already been deployed
    if (this->readDeploymentState()) {
        this->log_ACTIVITY_HI_DeploymentAlreadyComplete();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        return;
    }

    if (this->m_state != DeploymentState::IDLE) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
        return;
    }

    this->m_state = DeploymentState::QUIET_WAIT;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void AntennaDeployer ::DEPLOY_STOP_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (this->m_state == DeploymentState::IDLE) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        return;
    }

    this->ensureBurnwireStopped();
    this->resetDeploymentState();
    this->log_ACTIVITY_HI_DeployFinish(Components::DeployResult::DEPLOY_RESULT_ABORT, this->m_currentAttempt);

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void AntennaDeployer ::RESET_DEPLOYMENT_STATE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Fw::ParamValid is_valid;
    auto file_path = this->paramGet_DEPLOYED_STATE_FILE(is_valid);
    FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);

    (void)Os::FileSystem::removeFile(file_path.toChar());
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void AntennaDeployer ::SET_DEPLOYMENT_STATE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool deployed) {
    if (deployed) {
        this->writeDeploymentState();
    } else {
        Fw::ParamValid is_valid;
        auto file_path = this->paramGet_DEPLOYED_STATE_FILE(is_valid);
        FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);
        (void)Os::FileSystem::removeFile(file_path.toChar());
    }

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Internal helpers
// ----------------------------------------------------------------------

void AntennaDeployer ::startNextAttempt() {
    this->m_currentAttempt++;

    // Emit quiet time expired event if we're transitioning from QUIET_WAIT state
    // Do this before resetting m_ticksInState so we capture the actual elapsed time
    if (this->m_state == DeploymentState::QUIET_WAIT) {
        this->log_ACTIVITY_HI_QuietTimeExpired(this->m_ticksInState);
    }

    this->m_ticksInState = 0;
    this->m_successDetected = false;

    this->log_ACTIVITY_HI_DeployAttempt(this->m_currentAttempt);

    this->tlmWrite_DeployAttemptCount(this->m_totalAttempts);
    this->m_totalAttempts++;

    this->m_burnTicksThisAttempt = 0;

    if (this->isConnected_burnStart_OutputPort(0)) {
        this->burnStart_out(0);
    }

    this->m_state = DeploymentState::BURNING;
}

void AntennaDeployer ::handleQuietWaitTick() {
    this->m_ticksInState++;

    Fw::ParamValid valid;
    const U32 quietTime = this->paramGet_QUIET_TIME_SEC(valid);
    if (this->m_ticksInState >= quietTime) {
        this->startNextAttempt();
    }
}

void AntennaDeployer ::handleBurningTick() {
    this->m_ticksInState++;
    this->m_burnTicksThisAttempt = this->m_ticksInState;

    if (this->m_state != DeploymentState::BURNING) {
        return;
    }

    Fw::ParamValid valid;
    const U32 burnDuration = this->paramGet_BURN_DURATION_SEC(valid);
    if (this->m_ticksInState >= burnDuration) {
        this->ensureBurnwireStopped();
        this->logBurnSignalCount();

        if (this->m_successDetected) {
            this->finishDeployment(Components::DeployResult::DEPLOY_RESULT_SUCCESS);
            return;
        }

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

    if (this->m_successDetected) {
        this->finishDeployment(Components::DeployResult::DEPLOY_RESULT_SUCCESS);
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

    if (result == Components::DeployResult::DEPLOY_RESULT_SUCCESS) {
        this->log_ACTIVITY_HI_DeploySuccess(this->m_currentAttempt);

        // Mark antenna as deployed by writing state file
        this->writeDeploymentState();
    }

    this->log_ACTIVITY_HI_DeployFinish(result, this->m_currentAttempt);

    this->resetDeploymentState();
}

void AntennaDeployer ::resetDeploymentState() {
    this->m_state = DeploymentState::IDLE;
    this->m_currentAttempt = 0;
    this->m_ticksInState = 0;
    this->m_successDetected = false;
    this->m_lastDistanceValid = false;
    this->m_burnTicksThisAttempt = 0;
}

bool AntennaDeployer ::isDistanceWithinValidRange(F32 distance) {
    Fw::ParamValid topValid;
    const F32 top = this->paramGet_INVALID_THRESHOLD_TOP_CM(topValid);

    Fw::ParamValid bottomValid;
    const F32 bottom = this->paramGet_INVALID_THRESHOLD_BOTTOM_CM(bottomValid);

    return (distance <= top) && (distance >= bottom);
}

bool AntennaDeployer ::isDistanceDeployed(F32 distance) {
    Fw::ParamValid valid;
    const F32 threshold = this->paramGet_DEPLOYED_THRESHOLD_CM(valid);

    if (distance <= threshold) {
        this->m_successDetected = true;
        this->logBurnSignalCount();
        return true;
    }

    return false;
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
    Fw::ParamValid is_valid;
    auto file_path = this->paramGet_DEPLOYED_STATE_FILE(is_valid);
    FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);

    Os::File file;
    Os::File::Status status = file.open(file_path.toChar(), Os::File::OPEN_READ);
    bool deployed = (status == Os::File::OP_OK);
    (void)file.close();
    return deployed;
}

void AntennaDeployer ::writeDeploymentState() {
    Fw::ParamValid is_valid;
    auto file_path = this->paramGet_DEPLOYED_STATE_FILE(is_valid);
    FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);

    Os::File file;
    Os::File::Status status = file.open(file_path.toChar(), Os::File::OPEN_CREATE, Os::File::OVERWRITE);
    if (status == Os::File::OP_OK) {
        U8 marker = 1;
        FwSizeType size = sizeof(marker);
        (void)file.write(&marker, size);
    }
    (void)file.close();
}

}  // namespace Components
