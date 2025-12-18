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
        this->setMagnetorquers(false, false, false, false, false);
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

void DetumbleManager ::setDipoleMoment(Drv::DipoleMoment dpMoment) {
    // Calculate target currents
    F64 targetCurrent_x_plus = this->calculateTargetCurrent(dpMoment.get_x(), this->m_xPlusMagnetorquer);
    F64 targetCurrent_x_minus = this->calculateTargetCurrent(dpMoment.get_x(), this->m_xMinusMagnetorquer);
    F64 targetCurrent_y_plus = this->calculateTargetCurrent(dpMoment.get_y(), this->m_yPlusMagnetorquer);
    F64 targetCurrent_y_minus = this->calculateTargetCurrent(dpMoment.get_y(), this->m_yMinusMagnetorquer);
    F64 targetCurrent_z_minus = this->calculateTargetCurrent(dpMoment.get_z(), this->m_zMinusMagnetorquer);

    // Clamp currents
    F64 clampedCurrent_x_plus = this->clampCurrent(targetCurrent_x_plus, this->m_xPlusMagnetorquer);
    F64 clampedCurrent_x_minus = this->clampCurrent(targetCurrent_x_minus, this->m_xMinusMagnetorquer);
    F64 clampedCurrent_y_plus = this->clampCurrent(targetCurrent_y_plus, this->m_yPlusMagnetorquer);
    F64 clampedCurrent_y_minus = this->clampCurrent(targetCurrent_y_minus, this->m_yMinusMagnetorquer);
    F64 clampedCurrent_z_minus = this->clampCurrent(targetCurrent_z_minus, this->m_zMinusMagnetorquer);

    // All true for now until we figure out how to determine what should be on or off
    this->setMagnetorquers(true, true, true, true, true);
}

F64 DetumbleManager ::getAngularVelocityMagnitude(const Drv::AngularVelocity& angVel) {
    F64 magRadPerSec =
        std::sqrt(angVel.get_x() * angVel.get_x() + angVel.get_y() * angVel.get_y() + angVel.get_z() * angVel.get_z());

    return magRadPerSec * 180.0 / this->PI;
}

void DetumbleManager ::setMagnetorquers(bool x_plus, bool x_minus, bool y_plus, bool y_minus, bool z_minus) {
    this->xPlusToggle_out(0, x_plus);
    this->xMinusToggle_out(0, x_minus);
    this->yPlusToggle_out(0, y_plus);
    this->yMinusToggle_out(0, y_minus);
    this->zMinusToggle_out(0, z_minus);
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
    // First run call, initialize cooldown start time
    if (this->m_cooldownStartTime == Fw::ZERO_TIME) {
        this->m_cooldownStartTime = this->getTime();
    }

    // Get cooldown duration from parameter
    Fw::ParamValid isValid;
    Fw::TimeIntervalValue period = this->paramGet_COOLDOWN_DURATION(isValid);
    Fw::Time duration(this->m_cooldownStartTime.getTimeBase(), period.get_seconds(), period.get_useconds());
    Fw::Time cooldown_end_time = Fw::Time::add(this->m_cooldownStartTime, duration);

    // Check if cooldown period has elapsed and transition to SENSING state
    Fw::Time currentTime = this->getTime();
    if (currentTime >= cooldown_end_time) {
        // Transition to SENSING state
        this->m_detumbleState = DetumbleState::SENSING;
    }
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

    // Transition to TORQUING state
    this->m_torqueStartTime = this->getTime();
    this->m_detumbleState = DetumbleState::TORQUING;
}

void DetumbleManager ::stateTorquingActions() {
    // Check duration of torquing
    Fw::Time currentTime = this->getTime();

    // Get torque duration from parameter
    Fw::ParamValid isValid;
    Fw::TimeIntervalValue torque_duration_param = this->paramGet_TORQUE_DURATION(isValid);
    Fw::Time duration(this->m_torqueStartTime.getTimeBase(), torque_duration_param.get_seconds(),
                      torque_duration_param.get_useconds());
    Fw::Time torque_end_time = Fw::Time::add(this->m_torqueStartTime, duration);

    // Check if torquing duration has elapsed and transition to COOLDOWN state
    if (currentTime < torque_end_time) {
        this->setMagnetorquers(false, false, false, false, false);
        this->m_cooldownStartTime = this->getTime();
        this->m_detumbleState = DetumbleState::COOLDOWN;
        return;
    }

    // Perform torqueing action
    this->setDipoleMoment(this->m_dipole_moment);
}

}  // namespace Components
