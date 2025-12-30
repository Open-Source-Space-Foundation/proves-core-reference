// ======================================================================
// \title  BDot.cpp
// \brief  cpp file for BDot implementation class
// ======================================================================

#include "BDot.hpp"

#include <cerrno>
#include <cmath>

#include "Fw/Logger/Logger.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

BDot ::BDot() {}

BDot ::~BDot() {}

// ----------------------------------------------------------------------
//  public helper methods
// ----------------------------------------------------------------------

std::array<double, 3> BDot ::getDipoleMoment() {
    errno = 0;

    // Compute BDot
    std::array<double, 3> b_dot = this->computeBDot();

    // Compute magnitude of magnetic field
    double magnitude = this->getMagnitude();
    if (magnitude < 1e-6) {
        // Magnitude too small to compute dipole moment
        errno = EDOM;
        return std::array<double, 3>{0.0, 0.0, 0.0};
    }

    // Compute dipole moment components
    double moment_x = this->m_gain * b_dot[0] / magnitude;
    double moment_y = this->m_gain * b_dot[1] / magnitude;
    double moment_z = this->m_gain * b_dot[2] / magnitude;

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

    // TODO(nateinaction): Do we need to validate the time delta or just trust the data...?
    // if (this->m_sample_count > 1) {
    //     // Get previous sample timestamp
    //     std::chrono::microseconds previous_timestamp = this->m_sampling_set[this->m_sample_count - 1].timestamp;

    //     // Compute time delta between current and previous sample
    //     std::chrono::microseconds time_delta = timestamp - previous_timestamp;

    //     // Validate time delta
    //     if (!this->validateTimeDelta(time_delta)) {
    //         // Unable to add sample due to invalid time delta
    //         Fw::Logger::log("Invalid magnetometer sample delta when adding sample: %lld useconds\n",
    //         time_delta.count()); return;
    //     }
    // }

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

    // TODO(nateinaction): VALIDATE THIS IMPLEMENTATIONN
    // Linear fit over 5 equally spaced samples:
    // Bdot = (1 / (10 * Δt)) * Σ_{i=0..4} (i-2) * B_i
    double scale = 1.0 / (10.0 * dt_seconds);

    double sum_x = 0.0;
    double sum_y = 0.0;
    double sum_z = 0.0;

    for (std::size_t i = 0; i < SAMPLING_SET_SIZE; ++i) {
        int weight = static_cast<int>(i) - 2;  // -2, -1, 0, 1, 2
        sum_x += weight * this->m_sampling_set[i].magnetic_field[0];
        sum_y += weight * this->m_sampling_set[i].magnetic_field[1];
        sum_z += weight * this->m_sampling_set[i].magnetic_field[2];
    }

    return std::array<double, 3>{
        scale * sum_x,
        scale * sum_y,
        scale * sum_z,
    };
}

double BDot ::getMagnitude() const {
    std::array<double, 3> vector = this->m_sampling_set[this->m_sample_count - 1].magnetic_field;
    return std::sqrt(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);
}

// bool BDot ::validateTimeDelta(std::chrono::microseconds dt) {
//     // Check for out of order
//     if (dt.count() < 0) {
//         // Out of order samples, cannot compute dipole moment
//         Fw::Logger::log("Magnetometer samples out of order: %lld useconds\n", dt.count());
//         errno = EINVAL;
//         return false;
//     }

//     // Ensure magnetorquer samples are not faster than 100Hz magnetometer ODR
//     if (dt < this->m_magnetometer_sampling_period) {
//         // Out of order samples or samples taken too quickly, cannot compute dipole moment
//         Fw::Logger::log("Magnetometer samples too close together: %lld useconds\n", dt.count());
//         errno = EINVAL;
//         return false;
//     }

//     // TODO(nateinaction): The 20ms limitation needs to be recorded in documentation, this prevents detumble from
//     // working on slower rate groups Set previous magnetic field if last sample time was more than 20ms ago
//     if (dt > this->m_rate_group_max_period) {
//         // Too much time has passed since previous sample, cannot compute dipole moment
//         Fw::Logger::log("Magnetometer sample too old: %lld useconds\n", dt.count());
//         errno = EAGAIN;

//         // Reset sample set
//         this->emptySampleSet();
//         return false;
//     }

//     return true;
// }

}  // namespace Components
