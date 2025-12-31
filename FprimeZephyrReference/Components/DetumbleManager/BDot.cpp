// ======================================================================
// \title  BDot.cpp
// \brief  cpp file for BDot implementation class
// ======================================================================

#include "BDot.hpp"

#include <cmath>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

BDot ::BDot() {}

BDot ::~BDot() {}

// ----------------------------------------------------------------------
//  public helper methods
// ----------------------------------------------------------------------

std::array<double, 3> BDot ::getMagneticMoment() {
    // Compute BDot
    std::array<double, 3> b_dot = this->computeBDot();

    // Compute dipole moment components
    double moment_x = -this->m_gain * b_dot[0];
    double moment_y = -this->m_gain * b_dot[1];
    double moment_z = -this->m_gain * b_dot[2];

    // Return result
    return std::array<double, 3>{moment_x, moment_y, moment_z};
}

void BDot ::configure(double gain,
                      std::chrono::microseconds magnetometer_sampling_period,
                      std::chrono::microseconds rate_group_max_period) {
    this->m_gain = gain;
    this->m_magnetometer_sampling_period = magnetometer_sampling_period;
    this->m_rate_group_max_period = rate_group_max_period;
}

void BDot ::addSample(const std::array<double, 3>& magnetic_field, std::chrono::microseconds timestamp) {
    // Add sample if there is space
    if (this->m_sample_count >= SAMPLING_SET_SIZE) {
        return;
    }
    this->m_sampling_set[this->m_sample_count] = {magnetic_field, timestamp};
    this->m_sample_count++;
}

bool BDot ::samplingComplete() const {
    return this->m_sample_count >= SAMPLING_SET_SIZE;
}

std::chrono::microseconds BDot ::getTimeBetweenSamples() const {
    if (this->m_sample_count < 2) {
        return std::chrono::microseconds(0);
    }

    std::chrono::microseconds first_timestamp = this->m_sampling_set[0].timestamp;
    std::chrono::microseconds last_timestamp = this->m_sampling_set[this->m_sample_count - 1].timestamp;

    return last_timestamp - first_timestamp;
}

void BDot ::emptySampleSet() {
    this->m_sample_count = 0;
}

// ----------------------------------------------------------------------
//  Private helper methods
// ----------------------------------------------------------------------

std::array<double, 3> BDot ::computeBDot() const {
    // Get time delta between samples in seconds
    double dt_seconds = this->m_magnetometer_sampling_period.count() / 1e6;
    if (dt_seconds <= 0.0) {
        return std::array<double, 3>{0.0, 0.0, 0.0};
    }

    // Compute BDot using 5-point Central Difference Formula
    double x = (-1.0 * this->m_sampling_set[4].magnetic_field[0] + 8.0 * this->m_sampling_set[3].magnetic_field[0] +
                -8.0 * this->m_sampling_set[1].magnetic_field[0] + 1.0 * this->m_sampling_set[0].magnetic_field[0]) /
               (12.0 * dt_seconds);
    double y = (-1.0 * this->m_sampling_set[4].magnetic_field[1] + 8.0 * this->m_sampling_set[3].magnetic_field[1] +
                -8.0 * this->m_sampling_set[1].magnetic_field[1] + 1.0 * this->m_sampling_set[0].magnetic_field[1]) /
               (12.0 * dt_seconds);
    double z = (-1.0 * this->m_sampling_set[4].magnetic_field[2] + 8.0 * this->m_sampling_set[3].magnetic_field[2] +
                -8.0 * this->m_sampling_set[1].magnetic_field[2] + 1.0 * this->m_sampling_set[0].magnetic_field[2]) /
               (12.0 * dt_seconds);

    return std::array<double, 3>{x, y, z};
}

}  // namespace Components
