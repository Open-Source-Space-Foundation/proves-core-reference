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
    // ----------------------------------------------------------------------
    //  Private helper types
    // ----------------------------------------------------------------------

    //! Structure to hold magnetorquer coil parameters
    struct magnetorquerCoil {
        enum MagnetorquerCoilShape::T shape;

        bool enabled;
        F64 maxCurrent;
        F64 numTurns;
        F64 voltage;
        F64 resistance;

        // Dimensions
        F64 width;     // For Rectangular
        F64 length;    // For Rectangular
        F64 diameter;  // For Circular
    };

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

  private:
    // ----------------------------------------------------------------------
    //  Private helper methods
    // ----------------------------------------------------------------------

    //! Perform a single B-Dot control step.
    bool executeControlStep(std::string& reason);

    //! Compute the angular velocity magnitude in degrees per second.
    //! Formula: |ω| = sqrt(ωx² + ωy² + ωz²)
    //!
    //! ωx, ωy, ωz are the angular velocity components in rad/s
    //! Returns magnitude in deg/s
    F64 getAngularVelocityMagnitude(const Drv::AngularVelocity& angVel);

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

    //! Clamp the current to the maximum allowed for the coil.
    //! Formula: I_clamped = sign(I) * min(|I|, I_max)
    //!
    //! I_clamped is the clamped current (A)
    //! I is the target current (A)
    //! I_max is the maximum current (A)
    F64 clampCurrent(F64 current, const magnetorquerCoil& coil);

    //! Set the dipole moment by toggling the magnetorquers
    void setDipoleMoment(Drv::DipoleMoment dpMoment);

    //! Set the magnetorquers on or off based on the provided values
    void setMagnetorquers(bool x_plus, bool x_minus, bool y_plus, bool y_minus, bool z_minus);

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

    //! Mathematical constant pi
    const double PI = 3.14159265358979323846;

    //! Detumble state
    DetumbleState m_detumbleState = DetumbleState::COOLDOWN;

    //! X+ Coil parameters
    magnetorquerCoil m_xPlusMagnetorquer;

    //! X- Coil parameters
    magnetorquerCoil m_xMinusMagnetorquer;

    //! Y+ Coil parameters
    magnetorquerCoil m_yPlusMagnetorquer;

    //! Y- Coil parameters
    magnetorquerCoil m_yMinusMagnetorquer;

    //! Z- Coil parameters
    magnetorquerCoil m_zMinusMagnetorquer;

    //! Cooldown start time
    Fw::Time m_cooldownStartTime;

    //! Magnetorquer trigger start time
    Fw::Time m_torqueStartTime;

    //! Last dipole moment gathered
    Drv::DipoleMoment m_dipole_moment;
};

}  // namespace Components

#endif
