// ======================================================================
// \title  DetumbleManager.cpp
// \brief  cpp file for DetumbleManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/DetumbleManager/DetumbleManager.hpp"

#include <cerrno>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

DetumbleManager ::DetumbleManager(const char* const compName)
    : DetumbleManagerComponentBase(compName),
      m_bdot(),
      m_strategy_selector(),
      m_x_plus_magnetorquer(),
      m_x_minus_magnetorquer(),
      m_y_plus_magnetorquer(),
      m_y_minus_magnetorquer(),
      m_z_minus_magnetorquer() {
    // Compile-time verification that internal CoilShape enum matches FPP-generated enum
    static_assert(
        static_cast<U8>(Magnetorquer::CoilShape::CIRCULAR) == static_cast<U8>(Components::CoilShape::CIRCULAR),
        "Internal CoilShape::CIRCULAR value must match FPP enum");
    static_assert(
        static_cast<U8>(Magnetorquer::CoilShape::RECTANGULAR) == static_cast<U8>(Components::CoilShape::RECTANGULAR),
        "Internal CoilShape::RECTANGULAR value must match FPP enum");

    // Compile-time verification that internal DetumbleStrategy enum matches FPP-generated enum
    static_assert(
        static_cast<U8>(StrategySelector::Strategy::IDLE) == static_cast<U8>(Components::DetumbleStrategy::IDLE),
        "Internal DetumbleStrategy::IDLE value must match FPP enum");
    static_assert(
        static_cast<U8>(StrategySelector::Strategy::BDOT) == static_cast<U8>(Components::DetumbleStrategy::BDOT),
        "Internal DetumbleStrategy::BDOT value must match FPP enum");
    static_assert(static_cast<U8>(StrategySelector::Strategy::HYSTERESIS) ==
                      static_cast<U8>(Components::DetumbleStrategy::HYSTERESIS),
                  "Internal DetumbleStrategy::HYSTERESIS value must match FPP enum");
}

DetumbleManager ::~DetumbleManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void DetumbleManager ::run_handler(FwIndexType portNum, U32 context) {
    // Telemeter mode
    this->tlmWrite_Mode(this->m_mode);

    // Telemeter state
    this->tlmWrite_State(this->m_state);

    // If detumble is disabled, ensure magnetorquers are off and exit early
    if (this->m_mode == DetumbleMode::DISABLED) {
        if (this->m_state != DetumbleState::COOLDOWN) {
            this->stopMagnetorquers();
            this->m_state = DetumbleState::COOLDOWN;  // Reset state to COOLDOWN when re-enabled
        }
        return;
    }

    switch (this->m_state) {
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

void DetumbleManager ::setMode_handler(FwIndexType portNum, const Components::DetumbleMode& mode) {
    this->m_mode = mode;
}

void DetumbleManager ::systemModeChanged_handler(FwIndexType portNum, const Components::SystemMode& mode) {
    if (mode == Components::SystemMode::SAFE_MODE) {
        this->m_mode = DetumbleMode::DISABLED;
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void DetumbleManager ::SET_MODE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Components::DetumbleMode mode) {
    this->setMode_handler(0, mode);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
//  Public helper methods
// ----------------------------------------------------------------------

void DetumbleManager ::configure() {
    Fw::ParamValid isValid;

    // Initialize coil parameters from configuration parameters
    // X+ Coil
    m_x_plus_magnetorquer.m_voltage = this->paramGet_X_PLUS_VOLTAGE(isValid);
    m_x_plus_magnetorquer.m_resistance = this->paramGet_X_PLUS_RESISTANCE(isValid);
    m_x_plus_magnetorquer.m_turns = this->paramGet_X_PLUS_TURNS(isValid);
    m_x_plus_magnetorquer.m_direction_sign = Magnetorquer::DirectionSign::POSITIVE;
    CoilShape xPlus_shape = this->paramGet_X_PLUS_SHAPE(isValid);
    m_x_plus_magnetorquer.m_shape = static_cast<Magnetorquer::CoilShape>(static_cast<CoilShape::T>(xPlus_shape));
    m_x_plus_magnetorquer.m_width = this->paramGet_X_PLUS_WIDTH(isValid);
    m_x_plus_magnetorquer.m_length = this->paramGet_X_PLUS_LENGTH(isValid);
    this->tlmWrite_XPlusVoltage(m_x_plus_magnetorquer.m_voltage);
    this->tlmWrite_XPlusResistance(m_x_plus_magnetorquer.m_resistance);
    this->tlmWrite_XPlusTurns(m_x_plus_magnetorquer.m_turns);
    this->tlmWrite_XPlusShape(xPlus_shape);
    this->tlmWrite_XPlusWidth(m_x_plus_magnetorquer.m_width);
    this->tlmWrite_XPlusLength(m_x_plus_magnetorquer.m_length);

    // X- Coil
    m_x_minus_magnetorquer.m_voltage = this->paramGet_X_MINUS_VOLTAGE(isValid);
    m_x_minus_magnetorquer.m_resistance = this->paramGet_X_MINUS_RESISTANCE(isValid);
    m_x_minus_magnetorquer.m_turns = this->paramGet_X_MINUS_TURNS(isValid);
    m_x_minus_magnetorquer.m_direction_sign = Magnetorquer::DirectionSign::NEGATIVE;
    CoilShape xMinus_shape = this->paramGet_X_MINUS_SHAPE(isValid);
    m_x_minus_magnetorquer.m_shape = static_cast<Magnetorquer::CoilShape>(static_cast<CoilShape::T>(xMinus_shape));
    m_x_minus_magnetorquer.m_width = this->paramGet_X_MINUS_WIDTH(isValid);
    m_x_minus_magnetorquer.m_length = this->paramGet_X_MINUS_LENGTH(isValid);
    this->tlmWrite_XMinusVoltage(m_x_minus_magnetorquer.m_voltage);
    this->tlmWrite_XMinusResistance(m_x_minus_magnetorquer.m_resistance);
    this->tlmWrite_XMinusTurns(m_x_minus_magnetorquer.m_turns);
    this->tlmWrite_XMinusShape(xMinus_shape);
    this->tlmWrite_XMinusWidth(m_x_minus_magnetorquer.m_width);
    this->tlmWrite_XMinusLength(m_x_minus_magnetorquer.m_length);

    // Y+ Coil
    m_y_plus_magnetorquer.m_voltage = this->paramGet_Y_PLUS_VOLTAGE(isValid);
    m_y_plus_magnetorquer.m_resistance = this->paramGet_Y_PLUS_RESISTANCE(isValid);
    m_y_plus_magnetorquer.m_turns = this->paramGet_Y_PLUS_TURNS(isValid);
    m_y_plus_magnetorquer.m_direction_sign = Magnetorquer::DirectionSign::POSITIVE;
    CoilShape yPlus_shape = this->paramGet_Y_PLUS_SHAPE(isValid);
    m_y_plus_magnetorquer.m_shape = static_cast<Magnetorquer::CoilShape>(static_cast<CoilShape::T>(yPlus_shape));
    m_y_plus_magnetorquer.m_width = this->paramGet_Y_PLUS_WIDTH(isValid);
    m_y_plus_magnetorquer.m_length = this->paramGet_Y_PLUS_LENGTH(isValid);
    this->tlmWrite_YPlusVoltage(m_y_plus_magnetorquer.m_voltage);
    this->tlmWrite_YPlusResistance(m_y_plus_magnetorquer.m_resistance);
    this->tlmWrite_YPlusTurns(m_y_plus_magnetorquer.m_turns);
    this->tlmWrite_YPlusShape(yPlus_shape);
    this->tlmWrite_YPlusWidth(m_y_plus_magnetorquer.m_width);
    this->tlmWrite_YPlusLength(m_y_plus_magnetorquer.m_length);

    // Y- Coil
    m_y_minus_magnetorquer.m_voltage = this->paramGet_Y_MINUS_VOLTAGE(isValid);
    m_y_minus_magnetorquer.m_resistance = this->paramGet_Y_MINUS_RESISTANCE(isValid);
    m_y_minus_magnetorquer.m_turns = this->paramGet_Y_MINUS_TURNS(isValid);
    m_y_minus_magnetorquer.m_direction_sign = Magnetorquer::DirectionSign::NEGATIVE;
    CoilShape yMinus_shape = this->paramGet_Y_MINUS_SHAPE(isValid);
    m_y_minus_magnetorquer.m_shape = static_cast<Magnetorquer::CoilShape>(static_cast<CoilShape::T>(yMinus_shape));
    m_y_minus_magnetorquer.m_width = this->paramGet_Y_MINUS_WIDTH(isValid);
    m_y_minus_magnetorquer.m_length = this->paramGet_Y_MINUS_LENGTH(isValid);
    this->tlmWrite_YMinusVoltage(m_y_minus_magnetorquer.m_voltage);
    this->tlmWrite_YMinusResistance(m_y_minus_magnetorquer.m_resistance);
    this->tlmWrite_YMinusTurns(m_y_minus_magnetorquer.m_turns);
    this->tlmWrite_YMinusShape(yMinus_shape);
    this->tlmWrite_YMinusWidth(m_y_minus_magnetorquer.m_width);
    this->tlmWrite_YMinusLength(m_y_minus_magnetorquer.m_length);

    // Z- Coil
    m_z_minus_magnetorquer.m_voltage = this->paramGet_Z_MINUS_VOLTAGE(isValid);
    m_z_minus_magnetorquer.m_resistance = this->paramGet_Z_MINUS_RESISTANCE(isValid);
    m_z_minus_magnetorquer.m_turns = this->paramGet_Z_MINUS_TURNS(isValid);
    m_z_minus_magnetorquer.m_direction_sign = Magnetorquer::DirectionSign::NEGATIVE;
    CoilShape zMinus_shape = this->paramGet_Z_MINUS_SHAPE(isValid);
    m_z_minus_magnetorquer.m_shape = static_cast<Magnetorquer::CoilShape>(static_cast<CoilShape::T>(zMinus_shape));
    m_z_minus_magnetorquer.m_diameter = this->paramGet_Z_MINUS_DIAMETER(isValid);
    this->tlmWrite_ZMinusVoltage(m_z_minus_magnetorquer.m_voltage);
    this->tlmWrite_ZMinusResistance(m_z_minus_magnetorquer.m_resistance);
    this->tlmWrite_ZMinusTurns(m_z_minus_magnetorquer.m_turns);
    this->tlmWrite_ZMinusShape(zMinus_shape);
    this->tlmWrite_ZMinusDiameter(m_z_minus_magnetorquer.m_diameter);
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
    Fw::Time duration(this->m_cooldown_start_time.getTimeBase(), period.get_seconds(), period.get_useconds());
    Fw::Time cooldown_end_time = Fw::Time::add(this->m_cooldown_start_time, duration);

    // Check if cooldown period has elapsed and exit cooldown state
    Fw::Time currentTime = this->getTime();
    if (currentTime >= cooldown_end_time) {
        this->stateExitCooldownActions();
    }
}

void DetumbleManager ::stateEnterCooldownActions() {
    // On first call after state transition
    if (this->m_cooldown_start_time == Fw::ZERO_TIME) {
        // Record cooldown start time
        this->m_cooldown_start_time = this->getTime();
    }
}

void DetumbleManager ::stateExitCooldownActions() {
    // Reset cooldown start time
    this->m_cooldown_start_time = Fw::ZERO_TIME;

    // Transition to SENSING state
    this->m_state = DetumbleState::SENSING;
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

    this->m_strategy = static_cast<DetumbleStrategy::T>(detumble_strategy);
    this->tlmWrite_DetumbleStrategy(this->m_strategy);

    // No detumbling required, remain in SENSING state
    if (this->m_strategy == DetumbleStrategy::IDLE) {
        return;
    }

    // Transition to TORQUING state
    this->m_state = DetumbleState::TORQUING;
}

void DetumbleManager ::stateTorquingActions() {
    this->stateEnterTorquingActions();

    // Get torque duration from parameter
    Fw::ParamValid isValid;
    Fw::TimeIntervalValue torque_duration_param = this->paramGet_TORQUE_DURATION(isValid);
    this->tlmWrite_TorqueDuration(torque_duration_param);

    // Calculate torque end time
    Fw::Time duration(this->m_torque_start_time.getTimeBase(), torque_duration_param.get_seconds(),
                      torque_duration_param.get_useconds());
    Fw::Time torque_end_time = Fw::Time::add(this->m_torque_start_time, duration);

    // Check if torquing duration has elapsed and exit torquing state
    Fw::Time currentTime = this->getTime();
    if (currentTime >= torque_end_time) {
        this->stateExitTorquingActions();
    }
}

void DetumbleManager ::stateEnterTorquingActions() {
    // Only perform actions on first call after state transition
    if (this->m_torque_start_time != Fw::ZERO_TIME) {
        return;
    }

    // Perform torqueing action based on selected strategy
    switch (this->m_strategy) {
        case DetumbleStrategy::BDOT:
            this->bdotTorqueAction();
            break;
        case DetumbleStrategy::HYSTERESIS:
            this->hysteresisTorqueAction();
            break;
        default:
            // Invalid strategy, log error and transition back to SENSING state
            this->log_WARNING_LO_InvalidDetumbleStrategy(this->m_strategy);
            this->m_state = DetumbleState::SENSING;
            return;
    }

    // Record torque start time
    this->m_torque_start_time = this->getTime();
}

void DetumbleManager ::stateExitTorquingActions() {
    // Turn off magnetorquers
    this->stopMagnetorquers();

    // Reset torque start time
    this->m_torque_start_time = Fw::ZERO_TIME;

    // Transition to COOLDOWN state
    this->m_state = DetumbleState::COOLDOWN;
}

void DetumbleManager ::bdotTorqueAction() {
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
    errno = 0;  // Clear errno
    std::array<double, 3> magnetic_field_array = {magnetic_field.get_x(), magnetic_field.get_y(),
                                                  magnetic_field.get_z()};
    std::array<double, 3> dipole_moment =
        this->m_bdot.getDipoleMoment(magnetic_field_array, magnetic_field.get_timestamp().get_seconds(),
                                     magnetic_field.get_timestamp().get_useconds(), gain);

    // Check for errors in dipole moment computation
    int rc = errno;
    if (rc != 0) {
        switch (rc) {
            case EAGAIN:
                return;
            case EDOM:
                this->log_WARNING_LO_MagneticFieldTooSmallForDipoleMoment();
                return;
            case EINVAL:
                this->log_WARNING_LO_InvalidMagneticFieldReadingForDipoleMoment();
                return;
            default:
                this->log_WARNING_LO_UnknownDipoleMomentComputationError();
                return;
        }
    }
    this->log_WARNING_LO_MagneticFieldTooSmallForDipoleMoment_ThrottleClear();
    this->log_WARNING_LO_InvalidMagneticFieldReadingForDipoleMoment_ThrottleClear();
    this->log_WARNING_LO_UnknownDipoleMomentComputationError_ThrottleClear();

    // Perform torqueing action
    this->startMagnetorquers(this->m_x_plus_magnetorquer.dipoleMomentToCurrent(dipole_moment[0]),
                             this->m_x_minus_magnetorquer.dipoleMomentToCurrent(dipole_moment[0]),
                             this->m_y_plus_magnetorquer.dipoleMomentToCurrent(dipole_moment[1]),
                             this->m_y_minus_magnetorquer.dipoleMomentToCurrent(dipole_moment[1]),
                             this->m_z_minus_magnetorquer.dipoleMomentToCurrent(dipole_moment[2]));
}

void DetumbleManager ::hysteresisTorqueAction() {
    Fw::ParamValid isValid;
    HysteresisAxis axis = this->paramGet_HYSTERESIS_AXIS(isValid);
    this->tlmWrite_HysteresisAxis(axis);

    switch (axis) {
        case HysteresisAxis::X_AXIS:
            this->startMagnetorquers(127, -127, 0, 0, 0);
            return;
        case HysteresisAxis::Y_AXIS:
            this->startMagnetorquers(0, 0, 127, -127, 0);
            return;
        case HysteresisAxis::Z_AXIS:
            this->startMagnetorquers(0, 0, 0, 0, -127);
            return;
    }
}

}  // namespace Components
