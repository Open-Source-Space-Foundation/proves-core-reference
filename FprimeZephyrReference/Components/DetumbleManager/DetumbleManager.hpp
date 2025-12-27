// ======================================================================
// \title  DetumbleManager.hpp
// \brief  hpp file for DetumbleManager component implementation class
// ======================================================================

#ifndef Components_DetumbleManager_HPP
#define Components_DetumbleManager_HPP

#include <cmath>
#include <string>

#include "FprimeZephyrReference/Components/DetumbleManager/BDot.hpp"
#include "FprimeZephyrReference/Components/DetumbleManager/DetumbleManagerComponentAc.hpp"
#include "FprimeZephyrReference/Components/DetumbleManager/Magnetorquer.hpp"

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

    //! Compute the angular velocity magnitude in degrees per second.
    //! Formula: |ω| = sqrt(ωx² + ωy² + ωz²)
    //!
    //! ωx, ωy, ωz are the angular velocity components in rad/s
    //! Returns magnitude in deg/s
    F64 getAngularVelocityMagnitude(const Drv::AngularVelocity& angular_velocity);

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

    //! Actions to perform in the SENSING state
    void stateSensingActions();

    //! Actions to perform in the TORQUING state
    void stateTorquingActions();

    //! Actions to perform when entering the TORQUING state
    void stateEnterTorquingActions();

    //! Actions to perform when exiting the TORQUING state
    void stateExitTorquingActions();

    //! Convert MagnetorquerCoilShape enum to Magnetorquer::CoilShape enum
    Magnetorquer::CoilShape toCoilShape(MagnetorquerCoilShape shape);

  private:
    // ----------------------------------------------------------------------
    //  Private member variables
    // ----------------------------------------------------------------------

    const double PI = 3.14159265358979323846;  //!< Mathematical constant pi

    BDot m_BDot;  //!< B-Dot detumble algorithm class

    DetumbleState m_detumbleState = DetumbleState::COOLDOWN;  //!< Detumble state

    Magnetorquer m_xPlusMagnetorquer;   //!< X+ Coil parameters
    Magnetorquer m_xMinusMagnetorquer;  //!< X- Coil parameters
    Magnetorquer m_yPlusMagnetorquer;   //!< Y+ Coil parameters
    Magnetorquer m_yMinusMagnetorquer;  //!< Y- Coil parameters
    Magnetorquer m_zMinusMagnetorquer;  //!< Z- Coil parameters

    Fw::Time m_cooldownStartTime = Fw::ZERO_TIME;  //!< Cooldown start time
    Fw::Time m_torqueStartTime = Fw::ZERO_TIME;    //!< Torque start time
};

}  // namespace Components

#endif
