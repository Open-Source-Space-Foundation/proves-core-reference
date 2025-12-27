// ======================================================================
// \title  BDot.hpp
// \brief  hpp file for BDot implementation class
// ======================================================================

#pragma once

#include <array>
#include <chrono>
#include <cstdint>

using TimePoint = std::chrono::time_point<std::chrono::steady_clock, std::chrono::microseconds>;

namespace Components {

class BDot {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct BDot object
    BDot();

    //! Destroy BDot object
    ~BDot();

  public:
    // ----------------------------------------------------------------------
    // Public helper methods
    // ----------------------------------------------------------------------

    // Compute the required dipole moment to detumble the satellite.
    //
    // m = -k * (dB/dt) / |B|
    //
    // m is the dipole moment in A⋅m²
    // k is a gain constant
    // dB/dt is the time derivative of the magnetic field reading in micro-Tesla per second (uT/s)
    // |B| is the magnitude of the magnetic field vector in micro-Tesla (uT)
    std::array<double, 3> getDipoleMoment(
        std::array<double, 3> magnetic_field,  //!< Magnetic field vector
        uint32_t reading_seconds,              //!< Timestamp of reading: whole seconds since epoch
        uint32_t reading_useconds,             //!< Timestamp of reading: fractional part in microseconds [0, 999999]
        double gain                            //!< Gain constant
    );

  private:
    // ----------------------------------------------------------------------
    //  Private helper methods
    // ----------------------------------------------------------------------

    //! Compute the derivative of the magnetic field vector.
    std::array<double, 3> dB_dt(std::array<double, 3> magnetic_field,  //!< Magnetic field
                                std::chrono::microseconds dt_us  //!< Time delta between current and previous reading
    );

    //! Compute the magnitude of a 3D vector.
    double getMagnitude(std::array<double, 3> vector);

    //! Update previous magnetic field reading and timestamp
    void updatePreviousReading(std::array<double, 3> magnetic_field, TimePoint reading);

    //! Validate time delta between readings
    bool validateTimeDelta(std::chrono::microseconds dt);

  private:
    // ----------------------------------------------------------------------
    //  Private member variables
    // ----------------------------------------------------------------------

    std::array<double, 3> m_previous_magnetic_field;   //!< Previous magnetic field reading
    TimePoint m_previous_magnetic_field_reading_time;  //!< Time of previous reading
};

}  // namespace Components
