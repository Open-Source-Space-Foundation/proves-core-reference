// ======================================================================
// \title  DetumbleHelper.hpp
// \brief  hpp file for DetumbleHelper component implementation class
// ======================================================================

#pragma once

namespace Components {

class DetumbleHelper {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct DetumbleHelper object
    DetumbleHelper();

    //! Destroy DetumbleHelper object
    ~DetumbleHelper();

  public:
    // ----------------------------------------------------------------------
    //  Public helper methods
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
};

}  // namespace Components
