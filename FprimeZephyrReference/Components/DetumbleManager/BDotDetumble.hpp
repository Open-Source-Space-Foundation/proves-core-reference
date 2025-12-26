// ======================================================================
// \title  BDotDetumble.hpp
// \brief  hpp file for BDotDetumble component implementation class
// ======================================================================

#pragma once

#include <array>

namespace Components {

class BDotDetumble {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct BDotDetumble object
    BDotDetumble();

    //! Destroy BDotDetumble object
    ~BDotDetumble();

  public:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    // Compute the required dipole moment to detumble the satellite.
    //
    // m = -k * (dB/dt) / |B|
    //
    // m is the dipole moment in A⋅m²
    // k is a gain constant
    // dB/dt is the time derivative of the magnetic field reading in micro-Tesla per second (uT/s)
    // |B| is the magnitude of the magnetic field vector in micro-Tesla (uT)
    Drv::DipoleMoment getDipoleMomentGet(Fw::Success& condition) override;

    //! Computes the magnitude of the magnetic field vector.
    F64 getMagnitude(Drv::MagneticField magField);

    //! Compute the derivative of the magnetic field vector.
    std::array<F64, 3> dB_dt(Drv::MagneticField currMagField, Drv::MagneticField prevMagField);

    //! Get the time of the magnetic field reading
    Fw::Time magneticFieldReadingTime(const Drv::MagneticField magField);

  private:
    // ----------------------------------------------------------------------
    //  Private member variables
    // ----------------------------------------------------------------------
    Drv::MagneticField m_previousMagField;  //!< Previous magnetic field reading
};

}  // namespace Components
