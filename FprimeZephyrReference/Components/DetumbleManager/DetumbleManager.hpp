// ======================================================================
// \title  DetumbleManager.hpp
// \brief  hpp file for DetumbleManager component implementation class
// ======================================================================

#ifndef Components_DetumbleManager_HPP
#define Components_DetumbleManager_HPP

#include "FprimeZephyrReference/Components/DetumbleManager/BDot.hpp"
#include "FprimeZephyrReference/Components/DetumbleManager/DetumbleManagerComponentAc.hpp"
#include "FprimeZephyrReference/Components/DetumbleManager/Magnetorquer.hpp"
#include "FprimeZephyrReference/Components/DetumbleManager/StrategySelector.hpp"

namespace Components {

class DetumbleManager final : public DetumbleManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct DetumbleManager object
    DetumbleManager(const char* const compName);

    //! Destroy DetumbleManager object
    ~DetumbleManager();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    //!
    //! Run loop
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    //! Handler implementation for setMode
    //!
    //! Port for disabling detumbling
    void setMode_handler(FwIndexType portNum,                  //!< The port number
                         const Components::DetumbleMode& mode  //!< Desired detumble mode
                         ) override;

    //! Handler implementation for systemModeChanged
    //!
    //! Port for receiving system mode change notifications
    void systemModeChanged_handler(FwIndexType portNum,  //!< The port number
                                   const Components::SystemMode& mode) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command SET_MODE
    //!
    //! Command to set the operating mode
    void SET_MODE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                             U32 cmdSeq,           //!< The command sequence number
                             Components::DetumbleMode mode) override;

  public:
    // ----------------------------------------------------------------------
    //  Public helper methods
    // ----------------------------------------------------------------------

    //! Configure and initialize magnetorquer coils from parameters
    void configure();

  private:
    // ----------------------------------------------------------------------
    //  Private helper methods
    // ----------------------------------------------------------------------

    //! Turn the magnetorquers on based on the provided values
    void startMagnetorquers(I8 x_plus_drive_level,
                            I8 x_minus_drive_level,
                            I8 y_plus_drive_level,
                            I8 y_minus_drive_level,
                            I8 z_minus_drive_level);

    //! Turn the magnetorquers off based on the provided values
    void stopMagnetorquers();

    //! Actions to perform in the COOLDOWN state
    void stateCooldownActions();

    //! Actions to perform when entering the COOLDOWN state
    void stateEnterCooldownActions();

    //! Actions to perform when exiting the COOLDOWN state
    void stateExitCooldownActions();

    //! Actions to perform in the SENSING_ANGULAR_VELOCITY state
    void stateSensingAngularVelocityActions();

    //! Actions to perform when entering the SENSING_ANGULAR_VELOCITY state
    void stateEnterSensingAngularVelocityActions();

    //! Actions to perform when exiting the SENSING_ANGULAR_VELOCITY state
    void stateExitSensingAngularVelocityActions(F64 angular_velocity_magnitude_deg_sec);

    //! Actions to perform in the SENSING_MAGNETIC_FIELD state
    void stateSensingMagneticFieldActions();

    //! Actions to perform in the ACTUATING_BDOT state
    void stateActuatingBDotActions();

    //! Actions to perform when entering the ACTUATING_BDOT state
    void stateEnterActuatingBDotActions();

    //! Actions to perform when exiting the ACTUATING_BDOT state
    void stateExitActuatingBDotActions();

    //! Actions to perform in the ACTUATING_HYSTERESIS state
    void stateActuatingHysteresisActions();

  private:
    // ----------------------------------------------------------------------
    //  Private member variables
    // ----------------------------------------------------------------------

    BDot m_bdot;                           //!< B-Dot detumble algorithm class
    StrategySelector m_strategy_selector;  //!< Detumble helper class
    Magnetorquer m_x_plus_magnetorquer;    //!< X+ Coil parameters
    Magnetorquer m_x_minus_magnetorquer;   //!< X- Coil parameters
    Magnetorquer m_y_plus_magnetorquer;    //!< Y+ Coil parameters
    Magnetorquer m_y_minus_magnetorquer;   //!< Y- Coil parameters
    Magnetorquer m_z_minus_magnetorquer;   //!< Z- Coil parameters

    DetumbleMode m_mode = DetumbleMode::DISABLED;          //!< Detumble mode
    DetumbleState m_state = DetumbleState::COOLDOWN;       //!< Detumble state
    DetumbleStrategy m_strategy = DetumbleStrategy::IDLE;  //!< Detumble strategy

    Fw::Time m_cooldown_start_time = Fw::ZERO_TIME;  //!< Cooldown start time
    Fw::Time m_torque_start_time = Fw::ZERO_TIME;    //!< Torque start time

    Fw::Time last_cycle_time = Fw::ZERO_TIME;  //!< Time of last run cycle
};

}  // namespace Components

#endif
