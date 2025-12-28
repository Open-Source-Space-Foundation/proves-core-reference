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

DetumbleManager ::DetumbleManager(const char* const compName)
    : DetumbleManagerComponentBase(compName),
      m_bdot(),
      m_strategy_selector(),
      m_xPlusMagnetorquer(),
      m_xMinusMagnetorquer(),
      m_yPlusMagnetorquer(),
      m_yMinusMagnetorquer(),
      m_zMinusMagnetorquer() {}

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
    m_xPlusMagnetorquer.m_voltage = this->paramGet_X_PLUS_VOLTAGE(isValid);
    m_xPlusMagnetorquer.m_resistance = this->paramGet_X_PLUS_RESISTANCE(isValid);
    m_xPlusMagnetorquer.m_turns = this->paramGet_X_PLUS_TURNS(isValid);
    m_xPlusMagnetorquer.m_direction_sign = Magnetorquer::DirectionSign::POSITIVE;
    FpCoilShape xPlus_shape = this->paramGet_X_PLUS_SHAPE(isValid);
    m_xPlusMagnetorquer.m_shape = this->toCoilShape(xPlus_shape);
    m_xPlusMagnetorquer.m_width = this->paramGet_X_PLUS_WIDTH(isValid);
    m_xPlusMagnetorquer.m_length = this->paramGet_X_PLUS_LENGTH(isValid);
    this->tlmWrite_XPlusVoltage(m_xPlusMagnetorquer.m_voltage);
    this->tlmWrite_XPlusResistance(m_xPlusMagnetorquer.m_resistance);
    this->tlmWrite_XPlusTurns(m_xPlusMagnetorquer.m_turns);
    this->tlmWrite_XPlusShape(xPlus_shape);
    this->tlmWrite_XPlusWidth(m_xPlusMagnetorquer.m_width);
    this->tlmWrite_XPlusLength(m_xPlusMagnetorquer.m_length);

    // X- Coil
    m_xMinusMagnetorquer.m_voltage = this->paramGet_X_MINUS_VOLTAGE(isValid);
    m_xMinusMagnetorquer.m_resistance = this->paramGet_X_MINUS_RESISTANCE(isValid);
    m_xMinusMagnetorquer.m_turns = this->paramGet_X_MINUS_TURNS(isValid);
    m_xMinusMagnetorquer.m_direction_sign = Magnetorquer::DirectionSign::NEGATIVE;
    FpCoilShape xMinus_shape = this->paramGet_X_MINUS_SHAPE(isValid);
    m_xMinusMagnetorquer.m_shape = this->toCoilShape(xMinus_shape);
    m_xMinusMagnetorquer.m_width = this->paramGet_X_MINUS_WIDTH(isValid);
    m_xMinusMagnetorquer.m_length = this->paramGet_X_MINUS_LENGTH(isValid);
    this->tlmWrite_XMinusVoltage(m_xMinusMagnetorquer.m_voltage);
    this->tlmWrite_XMinusResistance(m_xMinusMagnetorquer.m_resistance);
    this->tlmWrite_XMinusTurns(m_xMinusMagnetorquer.m_turns);
    this->tlmWrite_XMinusShape(xMinus_shape);
    this->tlmWrite_XMinusWidth(m_xMinusMagnetorquer.m_width);
    this->tlmWrite_XMinusLength(m_xMinusMagnetorquer.m_length);

    // Y+ Coil
    m_yPlusMagnetorquer.m_voltage = this->paramGet_Y_PLUS_VOLTAGE(isValid);
    m_yPlusMagnetorquer.m_resistance = this->paramGet_Y_PLUS_RESISTANCE(isValid);
    m_yPlusMagnetorquer.m_turns = this->paramGet_Y_PLUS_TURNS(isValid);
    m_yPlusMagnetorquer.m_direction_sign = Magnetorquer::DirectionSign::POSITIVE;
    FpCoilShape yPlus_shape = this->paramGet_Y_PLUS_SHAPE(isValid);
    m_yPlusMagnetorquer.m_shape = this->toCoilShape(yPlus_shape);
    m_yPlusMagnetorquer.m_width = this->paramGet_Y_PLUS_WIDTH(isValid);
    m_yPlusMagnetorquer.m_length = this->paramGet_Y_PLUS_LENGTH(isValid);
    this->tlmWrite_YPlusVoltage(m_yPlusMagnetorquer.m_voltage);
    this->tlmWrite_YPlusResistance(m_yPlusMagnetorquer.m_resistance);
    this->tlmWrite_YPlusTurns(m_yPlusMagnetorquer.m_turns);
    this->tlmWrite_YPlusShape(yPlus_shape);
    this->tlmWrite_YPlusWidth(m_yPlusMagnetorquer.m_width);
    this->tlmWrite_YPlusLength(m_yPlusMagnetorquer.m_length);

    // Y- Coil
    m_yMinusMagnetorquer.m_voltage = this->paramGet_Y_MINUS_VOLTAGE(isValid);
    m_yMinusMagnetorquer.m_resistance = this->paramGet_Y_MINUS_RESISTANCE(isValid);
    m_yMinusMagnetorquer.m_turns = this->paramGet_Y_MINUS_TURNS(isValid);
    m_yMinusMagnetorquer.m_direction_sign = Magnetorquer::DirectionSign::NEGATIVE;
    FpCoilShape yMinus_shape = this->paramGet_Y_MINUS_SHAPE(isValid);
    m_yMinusMagnetorquer.m_shape = this->toCoilShape(yMinus_shape);
    m_yMinusMagnetorquer.m_width = this->paramGet_Y_MINUS_WIDTH(isValid);
    m_yMinusMagnetorquer.m_length = this->paramGet_Y_MINUS_LENGTH(isValid);
    this->tlmWrite_YMinusVoltage(m_yMinusMagnetorquer.m_voltage);
    this->tlmWrite_YMinusResistance(m_yMinusMagnetorquer.m_resistance);
    this->tlmWrite_YMinusTurns(m_yMinusMagnetorquer.m_turns);
    this->tlmWrite_YMinusShape(yMinus_shape);
    this->tlmWrite_YMinusWidth(m_yMinusMagnetorquer.m_width);
    this->tlmWrite_YMinusLength(m_yMinusMagnetorquer.m_length);

    // Z- Coil
    m_zMinusMagnetorquer.m_voltage = this->paramGet_Z_MINUS_VOLTAGE(isValid);
    m_zMinusMagnetorquer.m_resistance = this->paramGet_Z_MINUS_RESISTANCE(isValid);
    m_zMinusMagnetorquer.m_turns = this->paramGet_Z_MINUS_TURNS(isValid);
    m_zMinusMagnetorquer.m_direction_sign = Magnetorquer::DirectionSign::NEGATIVE;
    FpCoilShape zMinus_shape = this->paramGet_Z_MINUS_SHAPE(isValid);
    m_zMinusMagnetorquer.m_shape = this->toCoilShape(zMinus_shape);
    m_zMinusMagnetorquer.m_diameter = this->paramGet_Z_MINUS_DIAMETER(isValid);
    this->tlmWrite_ZMinusVoltage(m_zMinusMagnetorquer.m_voltage);
    this->tlmWrite_ZMinusResistance(m_zMinusMagnetorquer.m_resistance);
    this->tlmWrite_ZMinusTurns(m_zMinusMagnetorquer.m_turns);
    this->tlmWrite_ZMinusShape(zMinus_shape);
    this->tlmWrite_ZMinusDiameter(m_zMinusMagnetorquer.m_diameter);
}

// ----------------------------------------------------------------------
//  Private helper methods
// ----------------------------------------------------------------------

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
    // Get angular velocity thresholds from parameters
    Fw::ParamValid isValid;
    F64 bdot_max_threshold = this->paramGet_BDOT_MAX_THRESHOLD(isValid);
    F64 deadband_upper_threshold = this->paramGet_DEADBAND_UPPER_THRESHOLD(isValid);
    F64 deadband_lower_threshold = this->paramGet_DEADBAND_LOWER_THRESHOLD(isValid);

    // Telemeter thresholds
    this->tlmWrite_BdotMaxThreshold(bdot_max_threshold);
    this->tlmWrite_DeadbandUpperThreshold(deadband_upper_threshold);
    this->tlmWrite_DeadbandLowerThreshold(deadband_lower_threshold);

    // Configure strategy selector with updated thresholds
    this->m_strategy_selector.configure(bdot_max_threshold, deadband_upper_threshold, deadband_lower_threshold);

    // Get angular velocity
    Fw::Success condition;
    Drv::AngularVelocity angular_velocity = this->angularVelocityGet_out(0, condition);
    if (condition != Fw::Success::SUCCESS) {
        this->log_WARNING_LO_AngularVelocityRetrievalFailed();
        return;
    }
    this->log_WARNING_LO_AngularVelocityRetrievalFailed_ThrottleClear();

    // Select detumble strategy based on angular velocity
    std::array<double, 3> angular_velocity_array = {angular_velocity.get_x(), angular_velocity.get_y(),
                                                    angular_velocity.get_z()};
    StrategySelector::Strategy detumble_strategy =
        this->m_strategy_selector.fromAngularVelocity(angular_velocity_array);

    this->m_detumbleStrategy = this->toFpDetumbleStrategy(detumble_strategy);
    this->tlmWrite_DetumbleStrategy(this->m_detumbleStrategy);

    // No detumbling required, remain in SENSING state
    if (this->m_detumbleStrategy == FpDetumbleStrategy::IDLE) {
        return;
    }

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
    Fw::Success condition;
    Drv::MagneticField magnetic_field = this->magneticFieldGet_out(0, condition);
    if (condition != Fw::Success::SUCCESS) {
        this->log_WARNING_LO_MagneticFieldRetrievalFailed();
        return;
    }
    this->log_WARNING_LO_MagneticFieldRetrievalFailed_ThrottleClear();

    // Get gain parameter
    Fw::ParamValid isValid;
    F64 gain = this->paramGet_GAIN(isValid);
    this->tlmWrite_Gain(gain);

    // Get dipole moment
    int rc = errno;
    std::array<double, 3> magnetic_field_array = {magnetic_field.get_x(), magnetic_field.get_y(),
                                                  magnetic_field.get_z()};
    std::array<double, 3> dipole_moment =
        this->m_bdot.getDipoleMoment(magnetic_field_array, magnetic_field.get_timestamp().get_seconds(),
                                     magnetic_field.get_timestamp().get_useconds(), gain);

    if (rc == EAGAIN) {
        return;  // Not enough data yet
    }

    if (rc == EDOM) {
        this->log_WARNING_LO_MagneticFieldTooSmallForDipoleMoment();
        return;
    }
    this->log_WARNING_LO_MagneticFieldTooSmallForDipoleMoment_ThrottleClear();

    if (rc == EINVAL) {
        this->log_WARNING_LO_InvalidMagneticFieldReadingForDipoleMoment();
        return;
    }
    this->log_WARNING_LO_InvalidMagneticFieldReadingForDipoleMoment_ThrottleClear();

    if (rc != 0) {
        this->log_WARNING_LO_UnknownDipoleMomentComputationError();
        return;
    }
    this->log_WARNING_LO_UnknownDipoleMomentComputationError_ThrottleClear();

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

Magnetorquer::CoilShape DetumbleManager ::toCoilShape(FpCoilShape shape) {
    switch (static_cast<FpCoilShape::T>(shape)) {
        case FpCoilShape::CIRCULAR:
            return Magnetorquer::CoilShape::CIRCULAR;
        default:
            return Magnetorquer::CoilShape::RECTANGULAR;
    }
}

FpDetumbleStrategy DetumbleManager ::toFpDetumbleStrategy(StrategySelector::Strategy strategy) {
    switch (strategy) {
        case StrategySelector::Strategy::BDOT:
            return FpDetumbleStrategy::BDOT;
        case StrategySelector::Strategy::HYSTERESIS:
            return FpDetumbleStrategy::HYSTERESIS;
        default:
            return FpDetumbleStrategy::IDLE;
    }
}

}  // namespace Components
