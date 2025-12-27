// ======================================================================
// \title  DetumbleManager.cpp
// \brief  cpp file for DetumbleManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/DetumbleManager/DetumbleManager.hpp"

#include <Fw/Types/String.hpp>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <numbers>
#include <string>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

DetumbleManager ::DetumbleManager(const char* const compName) : DetumbleManagerComponentBase(compName), m_BDot() {}

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
    if (mode == DetumbleMode::DISABLED) {
        if (this->m_detumbleState != DetumbleState::COOLDOWN) {
            this->stopMagnetorquers();
            this->m_detumbleState = DetumbleState::COOLDOWN;  // Reset state to COOLDOWN when re-enabled
        }
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
//  Public helper methods
// ----------------------------------------------------------------------

void DetumbleManager ::configure() {
    Fw::ParamValid isValid;

    // Initialize coil parameters from configuration parameters
    // X+ Coil
    double xPlusMagnetorquer_voltage = this->paramGet_X_PLUS_VOLTAGE(isValid);
    double xPlusMagnetorquer_resistance = this->paramGet_X_PLUS_RESISTANCE(isValid);
    double xPlusMagnetorquer_turns = this->paramGet_X_PLUS_TURNS(isValid);
    MagnetorquerCoilShape xPlusMagnetorquer_shape = this->paramGet_X_PLUS_SHAPE(isValid);
    double xPlusMagnetorquer_width = this->paramGet_X_PLUS_WIDTH(isValid);
    double xPlusMagnetorquer_length = this->paramGet_X_PLUS_LENGTH(isValid);
    this->tlmWrite_XPlusVoltage(xPlusMagnetorquer_voltage);
    this->tlmWrite_XPlusResistance(xPlusMagnetorquer_resistance);
    this->tlmWrite_XPlusTurns(xPlusMagnetorquer_turns);
    this->tlmWrite_XPlusShape(xPlusMagnetorquer_shape);
    this->tlmWrite_XPlusWidth(xPlusMagnetorquer_width);
    this->tlmWrite_XPlusLength(xPlusMagnetorquer_length);
    this->m_xPlusMagnetorquer =
        Magnetorquer(xPlusMagnetorquer_voltage, xPlusMagnetorquer_resistance, xPlusMagnetorquer_turns,
                     Magnetorquer::DirectionSign::POSITIVE, this->toCoilShape(xPlusMagnetorquer_shape),
                     xPlusMagnetorquer_width, xPlusMagnetorquer_length, 0.0);

    // X- Coil
    double xMinusMagnetorquer_voltage = this->paramGet_X_MINUS_VOLTAGE(isValid);
    double xMinusMagnetorquer_resistance = this->paramGet_X_MINUS_RESISTANCE(isValid);
    double xMinusMagnetorquer_turns = this->paramGet_X_MINUS_TURNS(isValid);
    MagnetorquerCoilShape xMinusMagnetorquer_shape = this->paramGet_X_MINUS_SHAPE(isValid);
    double xMinusMagnetorquer_width = this->paramGet_X_MINUS_WIDTH(isValid);
    double xMinusMagnetorquer_length = this->paramGet_X_MINUS_LENGTH(isValid);
    this->tlmWrite_XMinusVoltage(xMinusMagnetorquer_voltage);
    this->tlmWrite_XMinusResistance(xMinusMagnetorquer_resistance);
    this->tlmWrite_XMinusTurns(xMinusMagnetorquer_turns);
    this->tlmWrite_XMinusShape(xMinusMagnetorquer_shape);
    this->tlmWrite_XMinusWidth(xMinusMagnetorquer_width);
    this->tlmWrite_XMinusLength(xMinusMagnetorquer_length);
    this->m_xMinusMagnetorquer =
        Magnetorquer(xMinusMagnetorquer_voltage, xMinusMagnetorquer_resistance, xMinusMagnetorquer_turns,
                     Magnetorquer::DirectionSign::NEGATIVE, this->toCoilShape(xMinusMagnetorquer_shape),
                     xMinusMagnetorquer_width, xMinusMagnetorquer_length, 0.0);

    // Y+ Coil
    double yPlusMagnetorquer_voltage = this->paramGet_Y_PLUS_VOLTAGE(isValid);
    double yPlusMagnetorquer_resistance = this->paramGet_Y_PLUS_RESISTANCE(isValid);
    double yPlusMagnetorquer_turns = this->paramGet_Y_PLUS_TURNS(isValid);
    MagnetorquerCoilShape yPlusMagnetorquer_shape = this->paramGet_Y_PLUS_SHAPE(isValid);
    double yPlusMagnetorquer_width = this->paramGet_Y_PLUS_WIDTH(isValid);
    double yPlusMagnetorquer_length = this->paramGet_Y_PLUS_LENGTH(isValid);
    this->tlmWrite_YPlusVoltage(yPlusMagnetorquer_voltage);
    this->tlmWrite_YPlusResistance(yPlusMagnetorquer_resistance);
    this->tlmWrite_YPlusTurns(yPlusMagnetorquer_turns);
    this->tlmWrite_YPlusShape(yPlusMagnetorquer_shape);
    this->tlmWrite_YPlusWidth(yPlusMagnetorquer_width);
    this->tlmWrite_YPlusLength(yPlusMagnetorquer_length);
    this->m_yPlusMagnetorquer =
        Magnetorquer(yPlusMagnetorquer_voltage, yPlusMagnetorquer_resistance, yPlusMagnetorquer_turns,
                     Magnetorquer::DirectionSign::POSITIVE, this->toCoilShape(yPlusMagnetorquer_shape),
                     yPlusMagnetorquer_width, yPlusMagnetorquer_length, 0.0);

    // Y- Coil
    double yMinusMagnetorquer_voltage = this->paramGet_Y_MINUS_VOLTAGE(isValid);
    double yMinusMagnetorquer_resistance = this->paramGet_Y_MINUS_RESISTANCE(isValid);
    double yMinusMagnetorquer_turns = this->paramGet_Y_MINUS_TURNS(isValid);
    MagnetorquerCoilShape yMinusMagnetorquer_shape = this->paramGet_Y_MINUS_SHAPE(isValid);
    double yMinusMagnetorquer_width = this->paramGet_Y_MINUS_WIDTH(isValid);
    double yMinusMagnetorquer_length = this->paramGet_Y_MINUS_LENGTH(isValid);
    this->tlmWrite_YMinusVoltage(yMinusMagnetorquer_voltage);
    this->tlmWrite_YMinusResistance(yMinusMagnetorquer_resistance);
    this->tlmWrite_YMinusTurns(yMinusMagnetorquer_turns);
    this->tlmWrite_YMinusShape(yMinusMagnetorquer_shape);
    this->tlmWrite_YMinusWidth(yMinusMagnetorquer_width);
    this->tlmWrite_YMinusLength(yMinusMagnetorquer_length);
    this->m_yMinusMagnetorquer =
        Magnetorquer(yMinusMagnetorquer_voltage, yMinusMagnetorquer_resistance, yMinusMagnetorquer_turns,
                     Magnetorquer::DirectionSign::NEGATIVE, this->toCoilShape(yMinusMagnetorquer_shape),
                     yMinusMagnetorquer_width, yMinusMagnetorquer_length, 0.0);

    // Z- Coil
    double zMinusMagnetorquer_voltage = this->paramGet_Z_MINUS_VOLTAGE(isValid);
    double zMinusMagnetorquer_resistance = this->paramGet_Z_MINUS_RESISTANCE(isValid);
    double zMinusMagnetorquer_turns = this->paramGet_Z_MINUS_TURNS(isValid);
    MagnetorquerCoilShape zMinusMagnetorquer_shape = this->paramGet_Z_MINUS_SHAPE(isValid);
    double zMinusMagnetorquer_diameter = this->paramGet_Z_MINUS_DIAMETER(isValid);
    this->tlmWrite_ZMinusVoltage(zMinusMagnetorquer_voltage);
    this->tlmWrite_ZMinusResistance(zMinusMagnetorquer_resistance);
    this->tlmWrite_ZMinusTurns(zMinusMagnetorquer_turns);
    this->tlmWrite_ZMinusShape(zMinusMagnetorquer_shape);
    this->tlmWrite_ZMinusDiameter(zMinusMagnetorquer_diameter);
    this->m_zMinusMagnetorquer =
        Magnetorquer(zMinusMagnetorquer_voltage, zMinusMagnetorquer_resistance, zMinusMagnetorquer_turns,
                     Magnetorquer::DirectionSign::NEGATIVE, this->toCoilShape(zMinusMagnetorquer_shape), 0.0, 0.0,
                     zMinusMagnetorquer_diameter);
}

// ----------------------------------------------------------------------
//  Private helper methods
// ----------------------------------------------------------------------

F64 DetumbleManager ::getAngularVelocityMagnitude(const Drv::AngularVelocity& angular_velocity) {
    F64 magRadPerSec = std::sqrt(angular_velocity.get_x() * angular_velocity.get_x() +
                                 angular_velocity.get_y() * angular_velocity.get_y() +
                                 angular_velocity.get_z() * angular_velocity.get_z());

    return magRadPerSec * 180.0 / this->PI;
}

void DetumbleManager ::startMagnetorquers(I8 x_plus_drive_level,
                                          I8 x_minus_drive_level,
                                          I8 y_plus_drive_level,
                                          I8 y_minus_drive_level,
                                          I8 z_minus_drive_level) {
    this->xPlusStart_out(0, x_plus_drive_level);
    this->xMinusStart_out(0, x_minus_drive_level);
    this->yPlusStart_out(0, y_plus_drive_level);
    this->yMinusStart_out(0, y_minus_drive_level);
    this->zMinusStart_out(0, z_minus_drive_level);
}

void DetumbleManager ::stopMagnetorquers() {
    this->xPlusStop_out(0);
    this->xMinusStop_out(0);
    this->yPlusStop_out(0);
    this->yMinusStop_out(0);
    this->zMinusStop_out(0);
}

void DetumbleManager ::stateCooldownActions() {
    this->stateEnterCooldownActions();

    // Get cooldown duration from parameter
    Fw::ParamValid isValid;
    Fw::TimeIntervalValue period = this->paramGet_COOLDOWN_DURATION(isValid);
    this->tlmWrite_CooldownDuration(period);

    // Calculate cooldown end time
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

    // Transition to TORQUING state
    this->m_detumbleState = DetumbleState::TORQUING;
}

void DetumbleManager ::stateTorquingActions() {
    this->stateEnterTorquingActions();

    // Get torque duration from parameter
    Fw::ParamValid isValid;
    Fw::TimeIntervalValue torque_duration_param = this->paramGet_TORQUE_DURATION(isValid);
    this->tlmWrite_TorqueDuration(torque_duration_param);

    // Calculate torque end time
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
    // Only perform actions on first call after state transition
    if (this->m_torqueStartTime != Fw::ZERO_TIME) {
        return;
    }

    // Get magnetic field for dipole moment calculation
    Drv::MagneticField magnetic_field = this->magneticFieldGet_out(0, condition);
    if (condition != Fw::Success::SUCCESS) {
        this->log_WARNING_LO_MagneticFieldRetrievalFailed();
        return;
    }
    this->log_WARNING_LO_MagneticFieldRetrievalFailed_ThrottleClear();

    // Get dipole moment
    errno rc;
    std::array<double, 3> magnetic_field_array = {magnetic_field.get_x(), magnetic_field.get_y(),
                                                  magnetic_field.get_z()};
    std::array<double, 3> dipole_moment =
        this->m_BDot.getDipoleMoment(magnetic_field_array, magnetic_field.get_timestamp().get_seconds(),
                                     magnetic_field.get_timestamp().get_useconds(), this->paramGet_BDOT_GAIN(isValid));
    if (errno != 0) {
        this->log_WARNING_LO_DipoleMomentRetrievalFailed(static_cast<U8>(rc));
        return;
    }
    this->log_WARNING_LO_DipoleMomentRetrievalFailed_ThrottleClear();

    // Perform torqueing action
    this->startMagnetorquers(this->m_xPlusMagnetorquer.dipoleMomentToCurrent(dipole_moment[0]),
                             this->m_xMinusMagnetorquer.dipoleMomentToCurrent(dipole_moment[0]),
                             this->m_yPlusMagnetorquer.dipoleMomentToCurrent(dipole_moment[1]),
                             this->m_yMinusMagnetorquer.dipoleMomentToCurrent(dipole_moment[1]),
                             this->m_zMinusMagnetorquer.dipoleMomentToCurrent(dipole_moment[2]));

    // Record torque start time
    this->m_torqueStartTime = this->getTime();
}

void DetumbleManager ::stateExitTorquingActions() {
    // Turn off magnetorquers
    this->stopMagnetorquers();

    // Reset torque start time
    this->m_torqueStartTime = Fw::ZERO_TIME;

    // Transition to COOLDOWN state
    this->m_detumbleState = DetumbleState::COOLDOWN;
}

Magnetorquer::CoilShape DetumbleManager ::toCoilShape(MagnetorquerCoilShape shape) {
    switch (static_cast<MagnetorquerCoilShape::T>(shape)) {
        case MagnetorquerCoilShape::CIRCULAR:
            return Magnetorquer::CoilShape::CIRCULAR;
        default:
            return Magnetorquer::CoilShape::RECTANGULAR;
    }
}

}  // namespace Components
