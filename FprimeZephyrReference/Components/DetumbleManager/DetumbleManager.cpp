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
        case DetumbleState::SENSING_ANGULAR_VELOCITY:
            this->stateSensingAngularVelocityActions();
            return;
        case DetumbleState::SENSING_MAGNETIC_FIELD:
            this->stateSensingMagneticFieldActions();
            return;
        case DetumbleState::ACTUATING_BDOT:
            this->stateActuatingBDotActions();
            return;
        case DetumbleState::ACTUATING_HYSTERESIS:
            this->stateActuatingHysteresisActions();
            return;
    }
}

void DetumbleManager ::setMode_handler(FwIndexType portNum, const Components::DetumbleMode& mode) {
    if (mode != DetumbleMode::DISABLED && this->getSystemMode_out(0) == Components::SystemMode::SAFE_MODE) {
        this->log_WARNING_LO_EnableFailedSafeMode();
        this->m_mode = DetumbleMode::DISABLED;
        return;
    }
    this->log_WARNING_LO_EnableFailedSafeMode_ThrottleClear();

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
    F64 x_turns = this->paramGet_X_TURNS(isValid);
    F64 y_turns = this->paramGet_Y_TURNS(isValid);
    F64 z_turns = this->paramGet_Z_TURNS(isValid);

    // X+ Coil
    m_x_plus_magnetorquer.m_voltage = this->paramGet_X_PLUS_VOLTAGE(isValid);
    m_x_plus_magnetorquer.m_resistance = this->paramGet_X_PLUS_RESISTANCE(isValid);
    m_x_plus_magnetorquer.m_turns = x_turns;
    m_x_plus_magnetorquer.m_direction_sign = Magnetorquer::DirectionSign::POSITIVE;
    CoilShape xPlus_shape = this->paramGet_X_PLUS_SHAPE(isValid);
    m_x_plus_magnetorquer.m_shape = static_cast<Magnetorquer::CoilShape>(static_cast<CoilShape::T>(xPlus_shape));
    m_x_plus_magnetorquer.m_width = this->paramGet_X_PLUS_WIDTH(isValid);
    m_x_plus_magnetorquer.m_length = this->paramGet_X_PLUS_LENGTH(isValid);

    // X- Coil
    m_x_minus_magnetorquer.m_voltage = this->paramGet_X_MINUS_VOLTAGE(isValid);
    m_x_minus_magnetorquer.m_resistance = this->paramGet_X_MINUS_RESISTANCE(isValid);
    m_x_minus_magnetorquer.m_turns = x_turns;
    m_x_minus_magnetorquer.m_direction_sign = Magnetorquer::DirectionSign::NEGATIVE;
    CoilShape xMinus_shape = this->paramGet_X_MINUS_SHAPE(isValid);
    m_x_minus_magnetorquer.m_shape = static_cast<Magnetorquer::CoilShape>(static_cast<CoilShape::T>(xMinus_shape));
    m_x_minus_magnetorquer.m_width = this->paramGet_X_MINUS_WIDTH(isValid);
    m_x_minus_magnetorquer.m_length = this->paramGet_X_MINUS_LENGTH(isValid);

    // Y+ Coil
    m_y_plus_magnetorquer.m_voltage = this->paramGet_Y_PLUS_VOLTAGE(isValid);
    m_y_plus_magnetorquer.m_resistance = this->paramGet_Y_PLUS_RESISTANCE(isValid);
    m_y_plus_magnetorquer.m_turns = y_turns;
    m_y_plus_magnetorquer.m_direction_sign = Magnetorquer::DirectionSign::POSITIVE;
    CoilShape yPlus_shape = this->paramGet_Y_PLUS_SHAPE(isValid);
    m_y_plus_magnetorquer.m_shape = static_cast<Magnetorquer::CoilShape>(static_cast<CoilShape::T>(yPlus_shape));
    m_y_plus_magnetorquer.m_width = this->paramGet_Y_PLUS_WIDTH(isValid);
    m_y_plus_magnetorquer.m_length = this->paramGet_Y_PLUS_LENGTH(isValid);

    // Y- Coil
    m_y_minus_magnetorquer.m_voltage = this->paramGet_Y_MINUS_VOLTAGE(isValid);
    m_y_minus_magnetorquer.m_resistance = this->paramGet_Y_MINUS_RESISTANCE(isValid);
    m_y_minus_magnetorquer.m_turns = y_turns;
    m_y_minus_magnetorquer.m_direction_sign = Magnetorquer::DirectionSign::NEGATIVE;
    CoilShape yMinus_shape = this->paramGet_Y_MINUS_SHAPE(isValid);
    m_y_minus_magnetorquer.m_shape = static_cast<Magnetorquer::CoilShape>(static_cast<CoilShape::T>(yMinus_shape));
    m_y_minus_magnetorquer.m_width = this->paramGet_Y_MINUS_WIDTH(isValid);
    m_y_minus_magnetorquer.m_length = this->paramGet_Y_MINUS_LENGTH(isValid);

    // Z- Coil
    m_z_minus_magnetorquer.m_voltage = this->paramGet_Z_MINUS_VOLTAGE(isValid);
    m_z_minus_magnetorquer.m_resistance = this->paramGet_Z_MINUS_RESISTANCE(isValid);
    m_z_minus_magnetorquer.m_turns = z_turns;
    m_z_minus_magnetorquer.m_direction_sign = Magnetorquer::DirectionSign::NEGATIVE;
    CoilShape zMinus_shape = this->paramGet_Z_MINUS_SHAPE(isValid);
    m_z_minus_magnetorquer.m_shape = static_cast<Magnetorquer::CoilShape>(static_cast<CoilShape::T>(zMinus_shape));
    m_z_minus_magnetorquer.m_diameter = this->paramGet_Z_MINUS_DIAMETER(isValid);
}

// ----------------------------------------------------------------------
//  Private helper methods
// ----------------------------------------------------------------------

void DetumbleManager ::startMagnetorquers(I8 x_plus_drive_level,
                                          I8 x_minus_drive_level,
                                          I8 y_plus_drive_level,
                                          I8 y_minus_drive_level,
                                          I8 z_minus_drive_level) {
    if (isConnected_xPlusStart_OutputPort(0)) {
        this->xPlusStart_out(0, x_plus_drive_level);
    }
    if (isConnected_xMinusStart_OutputPort(0)) {
        this->xMinusStart_out(0, x_minus_drive_level);
    }
    if (isConnected_yPlusStart_OutputPort(0)) {
        this->yPlusStart_out(0, y_plus_drive_level);
    }
    if (isConnected_yMinusStart_OutputPort(0)) {
        this->yMinusStart_out(0, y_minus_drive_level);
    }
    if (isConnected_zMinusStart_OutputPort(0)) {
        this->zMinusStart_out(0, z_minus_drive_level);
    }
}

void DetumbleManager ::stopMagnetorquers() {
    if (isConnected_xPlusStop_OutputPort(0)) {
        this->xPlusStop_out(0);
    }
    if (isConnected_xMinusStop_OutputPort(0)) {
        this->xMinusStop_out(0);
    }
    if (isConnected_yPlusStop_OutputPort(0)) {
        this->yPlusStop_out(0);
    }
    if (isConnected_yMinusStop_OutputPort(0)) {
        this->yMinusStop_out(0);
    }
    if (isConnected_zMinusStop_OutputPort(0)) {
        this->zMinusStop_out(0);
    }
}

void DetumbleManager ::parameterUpdated(FwPrmIdType id) {
    switch (id) {
        case DetumbleManager::PARAMID_BDOT_MAX_THRESHOLD: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_BDOT_MAX_THRESHOLD(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_BdotMaxThresholdParamSet(parameter);
                this->tlmWrite_BdotMaxThresholdParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_COOLDOWN_DURATION: {
            Fw::ParamValid is_valid;
            Fw::TimeIntervalValue parameter = this->paramGet_COOLDOWN_DURATION(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_CooldownDurationParamSet(parameter);
                this->tlmWrite_CooldownDurationParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_DEADBAND_UPPER_THRESHOLD: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_DEADBAND_UPPER_THRESHOLD(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_DeadbandUpperThresholdParamSet(parameter);
                this->tlmWrite_DeadbandUpperThresholdParam(parameter);
            }
        }
        case DetumbleManager::PARAMID_DEADBAND_LOWER_THRESHOLD: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_DEADBAND_LOWER_THRESHOLD(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_DeadbandLowerThresholdParamSet(parameter);
                this->tlmWrite_DeadbandLowerThresholdParam(parameter);
            }
        }
        case DetumbleManager::PARAMID_GAIN: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_GAIN(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_GainParamSet(parameter);
                this->tlmWrite_GainParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_HYSTERESIS_AXIS: {
            Fw::ParamValid is_valid;
            HysteresisAxis parameter = this->paramGet_HYSTERESIS_AXIS(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_HysteresisAxisParamSet(parameter);
                this->tlmWrite_HysteresisAxisParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_TORQUE_DURATION: {
            Fw::ParamValid is_valid;
            Fw::TimeIntervalValue parameter = this->paramGet_TORQUE_DURATION(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_TorqueDurationParamSet(parameter);
                this->tlmWrite_TorqueDurationParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_X_MINUS_LENGTH: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_X_MINUS_LENGTH(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_XMinusLengthParamSet(parameter);
                this->tlmWrite_XMinusLengthParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_X_MINUS_RESISTANCE: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_X_MINUS_RESISTANCE(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_XMinusResistanceParamSet(parameter);
                this->tlmWrite_XMinusResistanceParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_X_MINUS_SHAPE: {
            Fw::ParamValid is_valid;
            CoilShape parameter = this->paramGet_X_MINUS_SHAPE(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_XMinusShapeParamSet(parameter);
                this->tlmWrite_XMinusShapeParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_X_MINUS_VOLTAGE: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_X_MINUS_VOLTAGE(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_XMinusVoltageParamSet(parameter);
                this->tlmWrite_XMinusVoltageParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_X_MINUS_WIDTH: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_X_MINUS_WIDTH(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_XMinusWidthParamSet(parameter);
                this->tlmWrite_XMinusWidthParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_X_PLUS_LENGTH: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_X_PLUS_LENGTH(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_XPlusLengthParamSet(parameter);
                this->tlmWrite_XPlusLengthParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_X_PLUS_RESISTANCE: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_X_PLUS_RESISTANCE(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_XPlusResistanceParamSet(parameter);
                this->tlmWrite_XPlusResistanceParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_X_PLUS_SHAPE: {
            Fw::ParamValid is_valid;
            CoilShape parameter = this->paramGet_X_PLUS_SHAPE(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_XPlusShapeParamSet(parameter);
                this->tlmWrite_XPlusShapeParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_X_PLUS_VOLTAGE: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_X_PLUS_VOLTAGE(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_XPlusVoltageParamSet(parameter);
                this->tlmWrite_XPlusVoltageParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_X_PLUS_WIDTH: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_X_PLUS_WIDTH(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_XPlusWidthParamSet(parameter);
                this->tlmWrite_XPlusWidthParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_X_TURNS: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_X_TURNS(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_XTurnsParamSet(parameter);
                this->tlmWrite_XTurnsParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_Y_MINUS_LENGTH: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_Y_MINUS_LENGTH(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_YMinusLengthParamSet(parameter);
                this->tlmWrite_YMinusLengthParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_Y_MINUS_RESISTANCE: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_Y_MINUS_RESISTANCE(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_YMinusResistanceParamSet(parameter);
                this->tlmWrite_YMinusResistanceParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_Y_MINUS_SHAPE: {
            Fw::ParamValid is_valid;
            CoilShape parameter = this->paramGet_Y_MINUS_SHAPE(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_YMinusShapeParamSet(parameter);
                this->tlmWrite_YMinusShapeParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_Y_MINUS_VOLTAGE: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_Y_MINUS_VOLTAGE(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_YMinusVoltageParamSet(parameter);
                this->tlmWrite_YMinusVoltageParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_Y_MINUS_WIDTH: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_Y_MINUS_WIDTH(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_YMinusWidthParamSet(parameter);
                this->tlmWrite_YMinusWidthParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_Y_PLUS_LENGTH: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_Y_PLUS_LENGTH(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_YPlusLengthParamSet(parameter);
                this->tlmWrite_YPlusLengthParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_Y_PLUS_RESISTANCE: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_Y_PLUS_RESISTANCE(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_YPlusResistanceParamSet(parameter);
                this->tlmWrite_YPlusResistanceParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_Y_PLUS_SHAPE: {
            Fw::ParamValid is_valid;
            CoilShape parameter = this->paramGet_Y_PLUS_SHAPE(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_YPlusShapeParamSet(parameter);
                this->tlmWrite_YPlusShapeParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_Y_PLUS_VOLTAGE: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_Y_PLUS_VOLTAGE(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_YPlusVoltageParamSet(parameter);
                this->tlmWrite_YPlusVoltageParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_Y_PLUS_WIDTH: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_Y_PLUS_WIDTH(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_YPlusWidthParamSet(parameter);
                this->tlmWrite_YPlusWidthParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_Y_TURNS: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_Y_TURNS(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_YTurnsParamSet(parameter);
                this->tlmWrite_YTurnsParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_Z_MINUS_DIAMETER: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_Z_MINUS_DIAMETER(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_ZMinusDiameterParamSet(parameter);
                this->tlmWrite_ZMinusDiameterParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_Z_MINUS_RESISTANCE: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_Z_MINUS_RESISTANCE(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_ZMinusResistanceParamSet(parameter);
                this->tlmWrite_ZMinusResistanceParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_Z_MINUS_SHAPE: {
            Fw::ParamValid is_valid;
            CoilShape parameter = this->paramGet_Z_MINUS_SHAPE(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_ZMinusShapeParamSet(parameter);
                this->tlmWrite_ZMinusShapeParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_Z_MINUS_VOLTAGE: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_Z_MINUS_VOLTAGE(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_ZMinusVoltageParamSet(parameter);
                this->tlmWrite_ZMinusVoltageParam(parameter);
            }
        } break;
        case DetumbleManager::PARAMID_Z_TURNS: {
            Fw::ParamValid is_valid;
            F64 parameter = this->paramGet_Z_TURNS(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_ZTurnsParamSet(parameter);
                this->tlmWrite_ZTurnsParam(parameter);
            }
        } break;
        default:
            FW_ASSERT(0);
            break;  // Fallthrough from assert (static analysis)
    }
}

void DetumbleManager ::stateCooldownActions() {
    Fw::ParamValid isValid;

    // Perform actions upon entering COOLDOWN state
    this->stateEnterCooldownActions();

    // Get cooldown duration from parameter
    Fw::TimeIntervalValue cooldown_duration_param = this->paramGet_COOLDOWN_DURATION(isValid);

    // Calculate elapsed torquing duration
    Fw::Time current_time = this->getTime();
    Fw::TimeInterval cooldown_duration = Fw::TimeInterval(this->m_cooldown_start_time, current_time);
    Fw::TimeInterval required_cooldown_duration(cooldown_duration_param.get_seconds(),
                                                cooldown_duration_param.get_useconds());

    // Check if cooldown duration has elapsed and exit COOLDOWN state
    if (Fw::TimeInterval::compare(cooldown_duration, required_cooldown_duration) == Fw::TimeInterval::GT) {
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
    this->m_state = DetumbleState::SENSING_ANGULAR_VELOCITY;
}

void DetumbleManager ::stateSensingAngularVelocityActions() {
    Fw::Success condition;

    // Perform actions upon entering SENSING_ANGULAR_VELOCITY state
    this->stateEnterSensingAngularVelocityActions();

    // Get angular velocity
    F64 angular_velocity_magnitude_deg_sec =
        this->angularVelocityMagnitudeGet_out(0, condition, AngularUnit::DEG_PER_SEC);
    if (condition != Fw::Success::SUCCESS) {
        this->log_WARNING_LO_AngularVelocityRetrievalFailed();
        return;
    }
    this->log_WARNING_LO_AngularVelocityRetrievalFailed_ThrottleClear();

    this->tlmWrite_AngularVelocityMagnitude(angular_velocity_magnitude_deg_sec);

    // Select detumble strategy based on angular velocity
    StrategySelector::Strategy detumble_strategy =
        this->m_strategy_selector.fromAngularVelocityMagnitude(angular_velocity_magnitude_deg_sec);
    this->m_strategy = static_cast<DetumbleStrategy::T>(detumble_strategy);

    // Perform actions upon exiting SENSING_ANGULAR_VELOCITY state
    this->stateExitSensingAngularVelocityActions(angular_velocity_magnitude_deg_sec);
}

void DetumbleManager ::stateEnterSensingAngularVelocityActions() {
    Fw::ParamValid isValid;

    // Get angular velocity thresholds from parameters
    F64 bdot_max_threshold = this->paramGet_BDOT_MAX_THRESHOLD(isValid);
    F64 deadband_upper_threshold = this->paramGet_DEADBAND_UPPER_THRESHOLD(isValid);
    F64 deadband_lower_threshold = this->paramGet_DEADBAND_LOWER_THRESHOLD(isValid);

    // Configure strategy selector with updated thresholds
    this->m_strategy_selector.configure(bdot_max_threshold, deadband_upper_threshold, deadband_lower_threshold);
}

void DetumbleManager ::stateExitSensingAngularVelocityActions(F64 angular_velocity_magnitude_deg_sec) {
    switch (this->m_strategy) {
        case DetumbleStrategy::IDLE:
            // No detumbling required, remain in SENSING_ANGULAR_VELOCITY state
            // Telemeter detumble completion
            this->log_ACTIVITY_LO_DetumbleCompleted(angular_velocity_magnitude_deg_sec);
            this->log_ACTIVITY_LO_DetumbleStarted_ThrottleClear();
            return;
        case DetumbleStrategy::BDOT:
            this->m_state = DetumbleState::SENSING_MAGNETIC_FIELD;
            break;
        case DetumbleStrategy::HYSTERESIS:
            this->m_state = DetumbleState::ACTUATING_HYSTERESIS;
            break;
    }

    // Telemeter detumble start
    this->log_ACTIVITY_LO_DetumbleStarted(angular_velocity_magnitude_deg_sec);
    this->log_ACTIVITY_LO_DetumbleCompleted_ThrottleClear();
}

void DetumbleManager ::stateSensingMagneticFieldActions() {
    Fw::Success condition;

    // Get magnetic field
    Drv::MagneticField magnetic_field = this->magneticFieldGet_out(0, condition);
    if (condition != Fw::Success::SUCCESS) {
        this->log_WARNING_LO_MagneticFieldRetrievalFailed();
        return;
    }
    this->log_WARNING_LO_MagneticFieldRetrievalFailed_ThrottleClear();

    // Add magnetic field sample to B-Dot controller
    std::chrono::microseconds sample_time(magnetic_field.get_timestamp().get_seconds() * 1000000 +
                                          magnetic_field.get_timestamp().get_useconds());
    std::array<double, 3> magnetic_field_array = {magnetic_field.get_x(), magnetic_field.get_y(),
                                                  magnetic_field.get_z()};
    this->m_bdot.addSample(magnetic_field_array, sample_time);

    if (this->m_bdot.samplingComplete()) {
        // Sampling complete, transition to ACTUATING_BDOT state
        this->m_state = DetumbleState::ACTUATING_BDOT;
    }
}

void DetumbleManager ::stateActuatingBDotActions() {
    // Enter ACTUATING_BDOT state
    this->stateEnterActuatingBDotActions();

    // Get magnetic moment
    std::array<double, 3> magnetic_moment = this->m_bdot.getMagneticMoment();

    // Perform torqueing action
    this->startMagnetorquers(this->m_x_plus_magnetorquer.magneticMomentToCurrent(magnetic_moment[0]),
                             this->m_x_minus_magnetorquer.magneticMomentToCurrent(magnetic_moment[0]),
                             this->m_y_plus_magnetorquer.magneticMomentToCurrent(magnetic_moment[1]),
                             this->m_y_minus_magnetorquer.magneticMomentToCurrent(magnetic_moment[1]),
                             this->m_z_minus_magnetorquer.magneticMomentToCurrent(magnetic_moment[2]));

    // Exit ACTUATING_BDOT state
    this->stateExitActuatingBDotActions();
}

void DetumbleManager ::stateEnterActuatingBDotActions() {
    Fw::Success condition;
    Fw::ParamValid isValid;

    // Only perform actions on first call after state transition
    if (this->m_torque_start_time != Fw::ZERO_TIME) {
        return;
    }

    // Get gain parameter
    F64 gain = this->paramGet_GAIN(isValid);

    // Get magnetometer sampling period
    Fw::TimeIntervalValue sampling_period = this->magneticFieldSamplingPeriodGet_out(0, condition);
    if (condition != Fw::Success::SUCCESS) {
        this->log_WARNING_LO_MagneticFieldSamplingPeriodRetrievalFailed();
        return;
    }
    this->log_WARNING_LO_MagneticFieldSamplingPeriodRetrievalFailed_ThrottleClear();
    std::chrono::microseconds sampling_period_us(sampling_period.get_useconds());

    // Configure B-Dot controller
    this->m_bdot.configure(gain, sampling_period_us);

    // Record torque start time
    this->m_torque_start_time = this->getTime();
}

void DetumbleManager ::stateExitActuatingBDotActions() {
    Fw::ParamValid isValid;

    // Get torque duration from parameter
    Fw::TimeIntervalValue torque_duration_param = this->paramGet_TORQUE_DURATION(isValid);

    // Calculate elapsed torquing duration
    Fw::Time current_time = this->getTime();
    Fw::TimeInterval torque_duration = Fw::TimeInterval(this->m_torque_start_time, current_time);
    Fw::TimeInterval required_torque_duration(torque_duration_param.get_seconds(),
                                              torque_duration_param.get_useconds());

    // Check if torquing duration has elapsed
    if (Fw::TimeInterval::compare(torque_duration, required_torque_duration) != Fw::TimeInterval::GT) {
        // Torque duration not yet elapsed, remain in ACTUATING_BDOT state
        return;
    }

    // Empty B-Dot sample set for next detumble cycle
    this->m_bdot.emptySampleSet();

    // Turn off magnetorquers
    this->stopMagnetorquers();

    // Reset torque start time
    this->m_torque_start_time = Fw::ZERO_TIME;

    // Transition to COOLDOWN state
    this->m_state = DetumbleState::COOLDOWN;
}

void DetumbleManager ::stateActuatingHysteresisActions() {
    Fw::ParamValid isValid;

    // Get axis parameter
    HysteresisAxis axis = this->paramGet_HYSTERESIS_AXIS(isValid);

    // Perform torqueing action
    switch (axis) {
        case HysteresisAxis::X_AXIS:
            this->startMagnetorquers(127, -127, 0, 0, 0);
            break;
        case HysteresisAxis::Y_AXIS:
            this->startMagnetorquers(0, 0, 127, -127, 0);
            break;
        case HysteresisAxis::Z_AXIS:
            this->startMagnetorquers(0, 0, 0, 0, -127);
            break;
    }

    // Transition to COOLDOWN state
    this->m_state = DetumbleState::COOLDOWN;
}

}  // namespace Components
