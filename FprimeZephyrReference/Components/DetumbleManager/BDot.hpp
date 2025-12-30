// ======================================================================
// \title  BDot.hpp
// \brief  hpp file for BDot implementation class
// ======================================================================

#pragma once

#include <array>
#include <chrono>
#include <cstdint>

using TimePoint = std::chrono::time_point<std::chrono::steady_clock, std::chrono::microseconds>;

namespace {
constexpr std::size_t SAMPLING_SET_SIZE = 5;  //!< Number of samples
}

namespace Components {

class BDot {
  public:
    // ----------------------------------------------------------------------
    //  Public types
    // ----------------------------------------------------------------------

    //! Sample structure for magnetic field readings
    struct Sample {
        std::array<double, 3> magnetic_field;  //!< Magnetic field vector in gauss
        std::chrono::microseconds timestamp;   //!< Timestamp of the reading
    };

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
    std::array<double, 3> getDipoleMoment();

    //! Configure BDot parameters
    void configure(double gain,                                             //!< Gain constant
                   std::chrono::microseconds magnetometer_sampling_period,  //!< Magnetometer sampling period
                   std::chrono::microseconds rate_group_max_period          //!< Rate group maximum period
    );

    //! Adds a magnetic field sample set
    void addSample(const std::array<double, 3>& magnetic_field,  //!< Magnetic field vector in gauss
                   std::chrono::microseconds timestamp           //!< Timestamp of the reading
    );

    //! Tells the caller if the sample set is full
    bool samplingComplete();

    //! Time delta for all readings in the sample set
    std::chrono::microseconds getTimeBetweenReadings();

    //! Empties the sample set
    void emptySampleSet();

  private:
    // ----------------------------------------------------------------------
    //  Private helper methods
    // ----------------------------------------------------------------------

    //! Compute the derivative of the magnetic field vector.
    std::array<double, 3> dB_dt(
        std::array<double, 3> magnetic_field,  //!< Magnetic field
        std::chrono::microseconds dt_us        //!< Time delta between current and previous reading in microseconds
    );

    //! Compute the magnitude of a 3D vector.
    double getMagnitude(std::array<double, 3> vector  //!< 3D vector
    );

    //! Validate time delta between readings
    bool validateTimeDelta(
        std::chrono::microseconds dt  //!< Time delta between current and previous reading in microseconds
    );

  private:
    // ----------------------------------------------------------------------
    //  Private member variables
    // ----------------------------------------------------------------------

    double m_gain;                                             //!< Gain constant
    std::chrono::microseconds m_magnetometer_sampling_period;  //!< Magnetometer
    std::chrono::microseconds m_rate_group_max_period;         //!< Rate group maximum period

    std::array<Sample, SAMPLING_SET_SIZE> m_sampling_set{};  //!< Set of samples used to compute BDot
    std::size_t m_sample_count = 0;                          //!< Number of samples in the set
};

}  // namespace Components
