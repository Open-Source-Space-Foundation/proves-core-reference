// ======================================================================
// \title  DeploymentSequencer.cpp
// \brief  cpp file for DeploymentSequencer class
// ======================================================================

#include "PROVESFlightControllerReference/Components/AntennaDeployer/DeploymentSequencer.hpp"

namespace Components {

namespace {

DeploymentSequencer::Effect simpleEffect(DeploymentSequencer::EffectKind kind) {
    return DeploymentSequencer::Effect{kind, 0, 0, 0, DeploymentSequencer::Result::SUCCESS};
}

}  // namespace

// ----------------------------------------------------------------------
// Step
// ----------------------------------------------------------------------

bool DeploymentSequencer::Step ::add(const Effect& effect) {
    if (this->m_count >= MAX_EFFECTS) {
        return false;
    }
    this->m_effects[this->m_count] = effect;
    this->m_count++;
    return true;
}

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

DeploymentSequencer ::DeploymentSequencer()
    : m_state(State::IDLE), m_currentAttempt(0), m_ticksInState(0), m_totalAttempts(0), m_burnTicksThisAttempt(0) {}

DeploymentSequencer ::~DeploymentSequencer() {}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

bool DeploymentSequencer ::isBusy() const {
    return this->m_state != State::IDLE;
}

DeploymentSequencer::Step DeploymentSequencer ::start() {
    Step step;
    if (this->isBusy()) {
        return step;
    }

    this->m_currentAttempt = 0;
    this->m_ticksInState = 0;
    this->m_burnTicksThisAttempt = 0;
    this->startNextAttempt(step);
    return step;
}

DeploymentSequencer::Step DeploymentSequencer ::tick(const Params& params) {
    Step step;
    switch (this->m_state) {
        case State::IDLE:
            break;
        case State::BURNING:
            this->tickBurning(step, params);
            break;
        case State::RETRY_WAIT:
            this->tickRetryWait(step, params);
            break;
    }
    return step;
}

DeploymentSequencer::Step DeploymentSequencer ::stop() {
    Step step;
    if (this->isBusy()) {
        this->finish(step, Result::ABORT);
    }
    return step;
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

void DeploymentSequencer ::startNextAttempt(Step& step) {
    this->m_currentAttempt++;
    this->m_ticksInState = 0;

    Effect started = simpleEffect(EffectKind::ATTEMPT_STARTED);
    started.attempt = this->m_currentAttempt;
    started.attemptCount = this->m_totalAttempts;
    step.add(started);
    this->m_totalAttempts++;

    this->m_burnTicksThisAttempt = 0;
    step.add(simpleEffect(EffectKind::BURN_START));
    this->m_state = State::BURNING;
}

void DeploymentSequencer ::stopBurn(Step& step) {
    step.add(simpleEffect(EffectKind::BURN_STOP));

    if (this->m_burnTicksThisAttempt > 0U) {
        Effect count = simpleEffect(EffectKind::BURN_SIGNAL_COUNT);
        count.burnTicks = this->m_burnTicksThisAttempt;
        step.add(count);
        this->m_burnTicksThisAttempt = 0;
    }
}

void DeploymentSequencer ::finish(Step& step, Result result) {
    this->stopBurn(step);

    // A deployment that ran out of attempts is also recorded as deployed, so it is not burned again
    if (result == Result::SUCCESS || result == Result::FAILED) {
        Effect deployed = simpleEffect(EffectKind::MARK_DEPLOYED);
        deployed.attempt = this->m_currentAttempt;
        step.add(deployed);
    }

    Effect finished = simpleEffect(EffectKind::FINISHED);
    finished.attempt = this->m_currentAttempt;
    finished.result = result;
    step.add(finished);

    this->m_state = State::IDLE;
    this->m_currentAttempt = 0;
    this->m_ticksInState = 0;
    this->m_burnTicksThisAttempt = 0;
}

void DeploymentSequencer ::tickBurning(Step& step, const Params& params) {
    this->m_ticksInState++;
    this->m_burnTicksThisAttempt = this->m_ticksInState;

    if (this->m_ticksInState < params.burnDurationSec) {
        return;
    }

    this->stopBurn(step);

    if (this->m_currentAttempt >= params.maxAttempts) {
        this->finish(step, Result::FAILED);
        return;
    }

    this->m_state = State::RETRY_WAIT;
    this->m_ticksInState = 0;
}

void DeploymentSequencer ::tickRetryWait(Step& step, const Params& params) {
    this->m_ticksInState++;

    if (params.retryDelaySec != 0U && this->m_ticksInState < params.retryDelaySec) {
        return;
    }

    if (this->m_currentAttempt >= params.maxAttempts) {
        this->finish(step, Result::FAILED);
        return;
    }

    this->startNextAttempt(step);
}

}  // namespace Components
