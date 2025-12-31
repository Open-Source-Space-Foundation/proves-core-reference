// ======================================================================
// \title  Magnetorquer.hpp
// \brief  hpp file for Magnetorquer implementation class
// ======================================================================

#pragma once

#include <array>
#include <cstdint>

namespace Components {

class Magnetorquer {
  public:
    // ----------------------------------------------------------------------
    //  Public types
    // ----------------------------------------------------------------------

    enum class CoilShape { RECTANGULAR = 0, CIRCULAR = 1 };

    enum DirectionSign { POSITIVE = 1, NEGATIVE = -1 };

  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Magnetorquer object
    Magnetorquer();

    //! Destroy Magnetorquer object
    ~Magnetorquer();

  public:
    // ----------------------------------------------------------------------
    //  Public helper methods
    // ----------------------------------------------------------------------

    //! Calculate the target current required to generate a specific magnetic moment.
    std::int8_t magneticMomentToCurrent(
        double magnetic_moment_component  //<! Magnetic moment component (x, y, or z) in A·m²
    ) const;

  private:
    // ----------------------------------------------------------------------
    //  Private helper methods
    // ----------------------------------------------------------------------

    //! Compute the coil area based on its shape and dimensions.
    //! For Rectangular: A = w * l
    //! For Circular: A = π * (d/2)^2
    //!
    //! A is the area (m²)
    //! w is the width (m)
    //! l is the length (m)
    //! d is the diameter (m)
    double getCoilArea() const;

    //! Compute the maximum coil current based on its voltage and resistance.
    //!
    //! I_max = V / R
    //!
    //! I_max is the maximum current (A)
    //! V is the voltage (V)
    //! R is the resistance (Ω)
    double getMaxCoilCurrent() const;

    //! Compute the target current required to generate a specific dipole moment.
    //!
    //! I = m / (N * A)
    //!
    //! I is the current (A)
    //! m is the dipole moment (A·m²)
    //! N is the number of turns
    //! A is the coil area (m²)
    double computeTargetCurrent(double dipole_moment_component  //<! Dipole moment component (x, y, or z) in A·m²
    ) const;

    //! Clamp the target current to the maximum coil current
    //!
    //! I_clamped = sign(I) * min(|I|, I_max)
    //!
    //! I_clamped is the clamped current (A)
    //! I is the target current (A)
    //! I_max is the maximum current (A)
    double computeClampedCurrent(double target_current  //<! Target current (A)
    ) const;

    //! Scale the clamped current to int8_t range [-127, 127].
    //!
    //! I_scaled = round((I_clamped / I_max) * 127)
    //!
    //! I_scaled is the scaled current (int8_t)
    //! I_clamped is the clamped current (A)
    //! I_max is the maximum current (A)
    std::int8_t scaled8BitCurrent(double clampedCurrent  //<! Clamped current (A)
    ) const;

  public:
    // ----------------------------------------------------------------------
    //  Public member variables
    // ----------------------------------------------------------------------

    double m_turns;       //<! Number of turns in the coil
    double m_voltage;     //<! Voltage (V) supplied to the coil
    double m_resistance;  //<! Resistance (Ω) of the coil

    DirectionSign m_direction_sign;  //!< Direction sign of the coil (positive or negative)

    CoilShape m_shape;  //!< Shape of the coil

    // Dimensions
    double m_width;     //!< If the coil is Rectangular
    double m_length;    //!< If the coil is Rectangular
    double m_diameter;  //!< If the coil is Circular
};

}  // namespace Components
