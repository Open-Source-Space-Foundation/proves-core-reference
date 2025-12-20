// ======================================================================
// \title  DetumbleManager.cpp
// \brief  cpp file for DetumbleManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/DetumbleManager/DetumbleManager.hpp"

#include <Fw/Types/String.hpp>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

DetumbleManager ::DetumbleManager(const char* const compName) : DetumbleManagerComponentBase(compName) {
    Fw::ParamValid isValid;

    // TODO(nateinaction): Set these in a slow rate group instead of constructor to allow them to change at runtime
    // Initialize coil parameters from configuration parameters
    // X+ Coil
    this->m_xPlusMagnetorquer.enabled = this->paramGet_X_PLUS_ENABLED(isValid);
    this->m_xPlusMagnetorquer.voltage = this->paramGet_X_PLUS_VOLTAGE(isValid);
    this->m_xPlusMagnetorquer.resistance = this->paramGet_X_PLUS_RESISTANCE(isValid);
    this->m_xPlusMagnetorquer.numTurns = this->paramGet_X_PLUS_NUM_TURNS(isValid);
    this->m_xPlusMagnetorquer.shape = this->paramGet_X_PLUS_SHAPE(isValid);
    this->m_xPlusMagnetorquer.width = this->paramGet_X_PLUS_WIDTH(isValid);
    this->m_xPlusMagnetorquer.length = this->paramGet_X_PLUS_LENGTH(isValid);

    // X- Coil
    this->m_xMinusMagnetorquer.enabled = this->paramGet_X_MINUS_ENABLED(isValid);
    this->m_xMinusMagnetorquer.voltage = this->paramGet_X_MINUS_VOLTAGE(isValid);
    this->m_xMinusMagnetorquer.resistance = this->paramGet_X_MINUS_RESISTANCE(isValid);
    this->m_xMinusMagnetorquer.numTurns = this->paramGet_X_MINUS_NUM_TURNS(isValid);
    this->m_xMinusMagnetorquer.shape = this->paramGet_X_MINUS_SHAPE(isValid);
    this->m_xMinusMagnetorquer.width = this->paramGet_X_MINUS_WIDTH(isValid);
    this->m_xMinusMagnetorquer.length = this->paramGet_X_MINUS_LENGTH(isValid);

    // Y+ Coil
    this->m_yPlusMagnetorquer.enabled = this->paramGet_Y_PLUS_ENABLED(isValid);
    this->m_yPlusMagnetorquer.voltage = this->paramGet_Y_PLUS_VOLTAGE(isValid);
    this->m_yPlusMagnetorquer.resistance = this->paramGet_Y_PLUS_RESISTANCE(isValid);
    this->m_yPlusMagnetorquer.numTurns = this->paramGet_Y_PLUS_NUM_TURNS(isValid);
    this->m_yPlusMagnetorquer.shape = this->paramGet_Y_PLUS_SHAPE(isValid);
    this->m_yPlusMagnetorquer.width = this->paramGet_Y_PLUS_WIDTH(isValid);
    this->m_yPlusMagnetorquer.length = this->paramGet_Y_PLUS_LENGTH(isValid);

    // Y- Coil
    this->m_yMinusMagnetorquer.enabled = this->paramGet_Y_MINUS_ENABLED(isValid);
    this->m_yMinusMagnetorquer.voltage = this->paramGet_Y_MINUS_VOLTAGE(isValid);
    this->m_yMinusMagnetorquer.resistance = this->paramGet_Y_MINUS_RESISTANCE(isValid);
    this->m_yMinusMagnetorquer.numTurns = this->paramGet_Y_MINUS_NUM_TURNS(isValid);
    this->m_yMinusMagnetorquer.shape = this->paramGet_Y_MINUS_SHAPE(isValid);
    this->m_yMinusMagnetorquer.width = this->paramGet_Y_MINUS_WIDTH(isValid);
    this->m_yMinusMagnetorquer.length = this->paramGet_Y_MINUS_LENGTH(isValid);

    // Z- Coil
    this->m_zMinusMagnetorquer.enabled = this->paramGet_Z_MINUS_ENABLED(isValid);
    this->m_zMinusMagnetorquer.voltage = this->paramGet_Z_MINUS_VOLTAGE(isValid);
    this->m_zMinusMagnetorquer.resistance = this->paramGet_Z_MINUS_RESISTANCE(isValid);
    this->m_zMinusMagnetorquer.numTurns = this->paramGet_Z_MINUS_NUM_TURNS(isValid);
    this->m_zMinusMagnetorquer.shape = this->paramGet_Z_MINUS_SHAPE(isValid);
    this->m_zMinusMagnetorquer.diameter = this->paramGet_Z_MINUS_DIAMETER(isValid);
}

DetumbleManager ::~DetumbleManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void DetumbleManager ::run_handler(FwIndexType portNum, U32 context) {
    // Check operating mode
    Fw::ParamValid isValid;
    DetumbleMode mode = this->paramGet_OPERATING_MODE(isValid);

    // Telemeter mode
    this->tlmWrite_Mode(mode);

    // Telemeter state
    this->tlmWrite_State(this->m_detumbleState);

    // If detumble is disabled, ensure magnetorquers are off and exit early
    if (this->paramGet_OPERATING_MODE(isValid) == DetumbleMode::DISABLED) {
        // this->stopMagnetorquers();
        this->m_detumbleState = DetumbleState::COOLDOWN;  // Reset state to COOLDOWN when re-enabled
        return;
    }

    switch (this->m_detumbleState) {
        case DetumbleState::COOLDOWN:
            this->stateCooldownActions();
            return;
        case DetumbleState::SENSING:
            this->stateSensingActions();
            return;
        case DetumbleState::TORQUING:
            this->stateTorquingActions();
            return;
    }
}

// ----------------------------------------------------------------------
//  Private helper methods
// ----------------------------------------------------------------------

void DetumbleManager ::setDipoleMoment(Drv::DipoleMoment dipoleMoment) {
    // Calculate target currents
    F64 targetCurrent_x_plus = this->calculateTargetCurrent(dipoleMoment.get_x(), this->m_xPlusMagnetorquer);
    F64 targetCurrent_x_minus = this->calculateTargetCurrent(dipoleMoment.get_x(), this->m_xMinusMagnetorquer);
    F64 targetCurrent_y_plus = this->calculateTargetCurrent(dipoleMoment.get_y(), this->m_yPlusMagnetorquer);
    F64 targetCurrent_y_minus = this->calculateTargetCurrent(dipoleMoment.get_y(), this->m_yMinusMagnetorquer);
    F64 targetCurrent_z_minus = this->calculateTargetCurrent(dipoleMoment.get_z(), this->m_zMinusMagnetorquer);

    // Clamp currents
    F64 clampedCurrent_x_plus = this->clampCurrent(targetCurrent_x_plus, this->m_xPlusMagnetorquer);
    F64 clampedCurrent_x_minus = this->clampCurrent(targetCurrent_x_minus, this->m_xMinusMagnetorquer);
    F64 clampedCurrent_y_plus = this->clampCurrent(targetCurrent_y_plus, this->m_yPlusMagnetorquer);
    F64 clampedCurrent_y_minus = this->clampCurrent(targetCurrent_y_minus, this->m_yMinusMagnetorquer);
    F64 clampedCurrent_z_minus = this->clampCurrent(targetCurrent_z_minus, this->m_zMinusMagnetorquer);

    // Convert to int8_t values in range [-127, 127]
    // TODO(nateinaction): this is not correct
    // I8 x_plus_amps = static_cast<I8>(std::round((clampedCurrent_x_plus /
    // this->getMaxCoilCurrent(this->m_xPlusMagnetorquer)) * 127.0)); I8 x_minus_amps =
    // static_cast<I8>(std::round((clampedCurrent_x_minus / this->getMaxCoilCurrent(this->m_xMinusMagnetorquer)) *
    // 127.0)); I8 y_plus_amps = static_cast<I8>(std::round((clampedCurrent_y_plus /
    // this->getMaxCoilCurrent(this->m_yPlusMagnetorquer)) * 127.0)); I8 y_minus_amps =
    // static_cast<I8>(std::round((clampedCurrent_y_minus / this->getMaxCoilCurrent(this->m_yMinusMagnetorquer)) *
    // 127.0)); I8 z_minus_amps = static_cast<I8>(std::round((clampedCurrent_z_minus /
    // this->getMaxCoilCurrent(this->m_zMinusMagnetorquer)) * 127.0));

    Fw::ParamValid isValid;
    Fw::TimeIntervalValue torque_duration_param = this->paramGet_TORQUE_DURATION(isValid);
    U32 torque_duration_us =
        static_cast<U32>(torque_duration_param.get_seconds() * 1000000 + torque_duration_param.get_useconds());
    // TODO(nateinaction): Use calculated currents
    this->startMagnetorquers(torque_duration_us, 127, 127, 127, 127, 127);
}

F64 DetumbleManager ::getAngularVelocityMagnitude(const Drv::AngularVelocity& angVel) {
    F64 magRadPerSec =
        std::sqrt(angVel.get_x() * angVel.get_x() + angVel.get_y() * angVel.get_y() + angVel.get_z() * angVel.get_z());

    return magRadPerSec * 180.0 / this->PI;
}

void DetumbleManager ::startMagnetorquers(U32 duration_us,
                                          I8 x_plus_amps,
                                          I8 x_minus_amps,
                                          I8 y_plus_amps,
                                          I8 y_minus_amps,
                                          I8 z_minus_amps) {
    Fw::ParamValid isValid;
    Fw::Success condition;
    Fw::String name;

    if (this->m_xPlusMagnetorquer.enabled) {
        condition = this->xPlusStart_out(0, duration_us, x_plus_amps);
        if (condition != Fw::Success::SUCCESS) {
            name = "X+";
            this->log_WARNING_LO_MagnetorquerStartFailed(name);
        }
    }

    if (this->m_xMinusMagnetorquer.enabled) {
        condition = this->xMinusStart_out(0, duration_us, x_minus_amps);
        if (condition != Fw::Success::SUCCESS) {
            name = "X-";
            this->log_WARNING_LO_MagnetorquerStartFailed(name);
        }
    }

    if (this->m_yPlusMagnetorquer.enabled) {
        condition = this->yPlusStart_out(0, duration_us, y_plus_amps);
        if (condition != Fw::Success::SUCCESS) {
            name = "Y+";
            this->log_WARNING_LO_MagnetorquerStartFailed(name);
        }
    }

    if (this->m_yMinusMagnetorquer.enabled) {
        condition = this->yMinusStart_out(0, duration_us, y_minus_amps);
        if (condition != Fw::Success::SUCCESS) {
            name = "Y-";
            this->log_WARNING_LO_MagnetorquerStartFailed(name);
        }
    }

    if (this->m_zMinusMagnetorquer.enabled) {
        condition = this->zMinusStart_out(0, duration_us, z_minus_amps);
        if (condition != Fw::Success::SUCCESS) {
            name = "Z-";
            this->log_WARNING_LO_MagnetorquerStartFailed(name);
        }
    }
}

void DetumbleManager ::stopMagnetorquers() {
    Fw::ParamValid isValid;

    if (this->m_xPlusMagnetorquer.enabled) {
        this->xPlusStop_out(0);
    }

    if (this->m_xMinusMagnetorquer.enabled) {
        this->xMinusStop_out(0);
    }

    if (this->m_yPlusMagnetorquer.enabled) {
        this->yPlusStop_out(0);
    }

    if (this->m_yMinusMagnetorquer.enabled) {
        this->yMinusStop_out(0);
    }

    if (this->m_zMinusMagnetorquer.enabled) {
        this->zMinusStop_out(0);
    }
}

F64 DetumbleManager ::getCoilArea(const magnetorquerCoil& coil) {
    if (coil.shape == MagnetorquerCoilShape::CIRCULAR) {
        return this->PI * std::pow(coil.diameter / 2.0, 2.0);
    }
    // Default to Rectangular
    return coil.width * coil.length;
}

F64 DetumbleManager ::getMaxCoilCurrent(const magnetorquerCoil& coil) {
    if (coil.resistance == 0.0) {
        return 0.0;
    }
    return coil.voltage / coil.resistance;
}

F64 DetumbleManager ::calculateTargetCurrent(F64 dipoleMoment, const magnetorquerCoil& coil) {
    F64 area = this->getCoilArea(coil);
    if (coil.numTurns == 0.0 || area == 0.0) {
        return 0.0;
    }
    return dipoleMoment / (coil.numTurns * area);
}

F64 DetumbleManager ::clampCurrent(F64 current, const magnetorquerCoil& coil) {
    F64 maxCurrent = this->getMaxCoilCurrent(coil);
    if (std::fabs(current) > maxCurrent) {
        return (current > 0) ? maxCurrent : -maxCurrent;
    }
    return current;
}

void DetumbleManager ::stateCooldownActions() {
    this->stateEnterCooldownActions();

    // Get cooldown duration from parameter
    Fw::ParamValid isValid;
    Fw::TimeIntervalValue period = this->paramGet_COOLDOWN_DURATION(isValid);
    Fw::Time duration(this->m_cooldownStartTime.getTimeBase(), period.get_seconds(), period.get_useconds());
    Fw::Time cooldown_end_time = Fw::Time::add(this->m_cooldownStartTime, duration);

    // Check if cooldown period has elapsed and exit cooldown state
    Fw::Time currentTime = this->getTime();
    if (currentTime >= cooldown_end_time) {
        this->stateExitCooldownActions();
    }
}

void DetumbleManager ::stateEnterCooldownActions() {
    // On first call after state transition
    if (this->m_cooldownStartTime == Fw::ZERO_TIME) {
        // Record cooldown start time
        this->m_cooldownStartTime = this->getTime();
    }
}

void DetumbleManager ::stateExitCooldownActions() {
    // Reset cooldown start time
    this->m_cooldownStartTime = Fw::ZERO_TIME;

    // Transition to SENSING state
    this->m_detumbleState = DetumbleState::SENSING;
}

void DetumbleManager ::stateSensingActions() {
    // Get rotational threshold from parameter
    Fw::ParamValid isValid;
    F64 rotational_threshold = this->paramGet_ROTATIONAL_THRESHOLD(isValid);

    // Get current angular velocity magnitude
    Fw::Success condition;
    Drv::AngularVelocity angular_velocity = this->angularVelocityGet_out(0, condition);
    if (condition != Fw::Success::SUCCESS) {
        this->log_WARNING_LO_AngularVelocityRetrievalFailed();
        return;
    }
    this->log_WARNING_LO_AngularVelocityRetrievalFailed_ThrottleClear();

    F64 angular_velocity_magnitude = this->getAngularVelocityMagnitude(angular_velocity);

    // If angular velocity is below threshold, remain in SENSING state
    // TODO(nateinaction): Consider transitioning the mode to DISABLED if below threshold
    if (angular_velocity_magnitude < rotational_threshold) {
        this->tlmWrite_BelowRotationalThreshold(true);
        return;
    }
    this->tlmWrite_BelowRotationalThreshold(false);

    // Get dipole moment
    this->m_dipole_moment = this->dipoleMomentGet_out(0, condition);
    if (condition != Fw::Success::SUCCESS) {
        this->log_WARNING_LO_DipoleMomentRetrievalFailed();
        return;
    }
    this->log_WARNING_LO_DipoleMomentRetrievalFailed_ThrottleClear();

    // Transition to TORQUING state
    this->m_detumbleState = DetumbleState::TORQUING;
}

void DetumbleManager ::stateTorquingActions() {
    this->stateEnterTorquingActions();

    // Get torque duration from parameter
    Fw::ParamValid isValid;
    Fw::TimeIntervalValue torque_duration_param = this->paramGet_TORQUE_DURATION(isValid);
    Fw::Time duration(this->m_torqueStartTime.getTimeBase(), torque_duration_param.get_seconds(),
                      torque_duration_param.get_useconds());
    Fw::Time torque_end_time = Fw::Time::add(this->m_torqueStartTime, duration);

    // Check if torquing duration has elapsed and exit torquing state
    Fw::Time currentTime = this->getTime();
    if (currentTime >= torque_end_time) {
        this->stateExitTorquingActions();
    }
}

void DetumbleManager ::stateEnterTorquingActions() {
    // On first call after state transition
    if (this->m_torqueStartTime == Fw::ZERO_TIME) {
        // Perform torqueing action
        this->setDipoleMoment(this->m_dipole_moment);

        // Record torque start time
        this->m_torqueStartTime = this->getTime();
    }
}

void DetumbleManager ::stateExitTorquingActions() {
    // Turn off magnetorquers
    this->stopMagnetorquers();

    // Reset torque start time
    this->m_torqueStartTime = Fw::ZERO_TIME;

    // Transition to COOLDOWN state
    this->m_detumbleState = DetumbleState::COOLDOWN;
}

}  // namespace Components
