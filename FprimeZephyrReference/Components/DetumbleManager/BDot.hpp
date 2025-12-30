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
constexpr std::size_t SAMPLING_SET_SIZE =
    5;  //!< Number of samples, to change this you must also change the computeBDot method
}

namespace Components {

class BDot {
  public:
    // ----------------------------------------------------------------------
    //  Public types
    // ----------------------------------------------------------------------

    //! Sample structure for magnetic field samples
    struct Sample {
        std::array<double, 3> magnetic_field;  //!< Magnetic field vector in gauss
        std::chrono::microseconds timestamp;   //!< Timestamp of the sample
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

    // Compute the required magnetic moment to detumble the satellite.
    //
    // m = -k * Ḃ
    //
    // m is the magnetic moment in A⋅m²
    // k is a gain constant in A⋅m²⋅s/G
    // Ḃ is the time derivative of the magnetic field sample in gauss per second (G/s)
    std::array<double, 3> getMagneticMoment();

    //! Configure BDot parameters
    void configure(double gain,                                             //!< Gain constant
                   std::chrono::microseconds magnetometer_sampling_period,  //!< Magnetometer sampling period
                   std::chrono::microseconds rate_group_max_period          //!< Rate group maximum period
    );

    //! Adds a magnetic field sample set
    void addSample(const std::array<double, 3>& magnetic_field,  //!< Magnetic field vector in gauss
                   std::chrono::microseconds timestamp           //!< Timestamp of the sample
    );

    //! Tells the caller if the sample set is full
    bool samplingComplete() const;

    //! Time delta for all samples in the set
    std::chrono::microseconds getTimeBetweenSamples() const;

    //! Empties the sample set
    void emptySampleSet();

  private:
    // ----------------------------------------------------------------------
    //  Private helper methods
    // ----------------------------------------------------------------------

    //! Compute BDot uses the central difference method to estimate the time derivative of the magnetic field.
    //!
    //! Ḃ = (-1)B_4 + 8B_3 - 8B_1 + B_0) / (12 * 𝚫t)
    //!
    //! B_i is the magnetic field vector at sample i
    //! 𝚫t is the time delta between samples in seconds
    //! TODO(evanjellison): Write up on coefficient derivation
    //! Coefficients (-1, 8, 12, etc.)
    std::array<double, 3> computeBDot() const;

    //! Compute the magnitude of the most recent magnetic field sample.
    double getMagnitude() const;

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
