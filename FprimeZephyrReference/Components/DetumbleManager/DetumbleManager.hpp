// ======================================================================
// \title  DetumbleManager.hpp
// \brief  hpp file for DetumbleManager component implementation class
// ======================================================================

#ifndef Components_DetumbleManager_HPP
#define Components_DetumbleManager_HPP

#include <cmath>
#include <string>

#include "FprimeZephyrReference/Components/DetumbleManager/DetumbleManagerComponentAc.hpp"

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

    //! Compute the coil area based on its shape and dimensions.
    //! For Rectangular: A = w * l
    //! For Circular: A = π * (d/2)^2
    //!
    //! A is the area (m²)
    //! w is the width (m)
    //! l is the length (m)
    //! d is the diameter (m)
    F64 getCoilArea(const magnetorquerCoil& coil);

    //! Compute the maximum coil current based on its voltage and resistance.
    //! Formula: I_max = V / R
    //!
    //! I_max is the maximum current (A)
    //! V is the voltage (V)
    //! R is the resistance (Ω)
    F64 getMaxCoilCurrent(const magnetorquerCoil& coil);

    //! Calculate the target current required to generate a specific dipole moment.
    //! Formula: I = m / (N * A)
    //!
    //! I is the current (A)
    //! m is the dipole moment (A·m²)
    //! N is the number of turns
    //! A is the coil area (m²)
    F64 calculateTargetCurrent(F64 dipoleMoment, const magnetorquerCoil& coil);

    //! Clamp the current to the maximum for the coil and scale to int8_t range [-127, 127].
    //! Formula: I_clamped = sign(I) * min(|I|, I_max)
    //!
    //! I_clamped is the clamped current (A)
    //! I is the target current (A)
    //! I_max is the maximum current (A)
    I8 clampCurrent(F64 targetCurrent, const magnetorquerCoil& coil);

    //! Set the dipole moment by toggling the magnetorquers
    void setDipoleMoment(Drv::DipoleMoment dipoleMoment);

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

  private:
    // ----------------------------------------------------------------------
    //  Private member variables
    // ----------------------------------------------------------------------

    const double PI = 3.14159265358979323846;  //!< Mathematical constant pi

    BDot m_BDot;  //!< B-Dot detumble algorithm class

    DetumbleState m_detumbleState = DetumbleState::COOLDOWN;  //!< Detumble state

    magnetorquerCoil m_xPlusMagnetorquer;   //!< X+ Coil parameters
    magnetorquerCoil m_xMinusMagnetorquer;  //!< X- Coil parameters
    magnetorquerCoil m_yPlusMagnetorquer;   //!< Y+ Coil parameters
    magnetorquerCoil m_yMinusMagnetorquer;  //!< Y- Coil parameters
    magnetorquerCoil m_zMinusMagnetorquer;  //!< Z- Coil parameters

    Fw::Time m_cooldownStartTime = Fw::ZERO_TIME;  //!< Cooldown start time
    Fw::Time m_torqueStartTime = Fw::ZERO_TIME;    //!< Torque start time
};

}  // namespace Components

#endif
