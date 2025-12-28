// ======================================================================
// \title  BDot.cpp
// \brief  cpp file for BDot implementation class
// ======================================================================

#include "BDot.hpp"

#include <errno.h>

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

std::array<double, 3> BDot ::getDipoleMoment(std::array<double, 3> magnetic_field,
                                             uint32_t reading_seconds,
                                             uint32_t reading_useconds,
                                             double gain) {
    errno = 0;

    // Compute the time delta between current and previous reading
    TimePoint reading_time =
        TimePoint{std::chrono::seconds(reading_seconds) + std::chrono::microseconds(reading_useconds)};
    std::chrono::microseconds dt_us = reading_time - this->m_previous_magnetic_field_reading_time;

    // Validate time delta
    if (!this->validateTimeDelta(dt_us)) {
        // Unable to compute dipole moment
        // If errno is set to EAGAIN, this is the first reading or too much time has passed
        // If errno is set to EINVAL, readings are out of order or too close together

        // Update previous magnetic field
        this->updatePreviousReading(magnetic_field, reading_time);

        // Return zero dipole moment
        return std::array<double, 3>{0.0, 0.0, 0.0};
    }

    // Compute dB/dt
    std::array<double, 3> dB_dt = this->dB_dt(magnetic_field, dt_us);

    // Compute magnitude of magnetic field
    double magnitude = this->getMagnitude(magnetic_field);
    if (magnitude < 1e-6) {
        // Magnitude too small to compute dipole moment
        errno = EDOM;
        return std::array<double, 3>{0.0, 0.0, 0.0};
    }

    // Compute dipole moment components
    double moment_x = gain * dB_dt[0] / magnitude;
    double moment_y = gain * dB_dt[1] / magnitude;
    double moment_z = gain * dB_dt[2] / magnitude;

    // Update previous magnetic field
    this->updatePreviousReading(magnetic_field, reading_time);

    // Return result
    return std::array<double, 3>{moment_x, moment_y, moment_z};
}

// ----------------------------------------------------------------------
//  Private helper methods
// ----------------------------------------------------------------------

std::array<double, 3> BDot ::dB_dt(std::array<double, 3> magnetic_field, std::chrono::microseconds dt_us) {
    // Extract components for readability
    double magnetic_field_x = magnetic_field[0];
    double magnetic_field_y = magnetic_field[1];
    double magnetic_field_z = magnetic_field[2];
    double previous_magnetic_field_x = this->m_previous_magnetic_field[0];
    double previous_magnetic_field_y = this->m_previous_magnetic_field[1];
    double previous_magnetic_field_z = this->m_previous_magnetic_field[2];

    // Convert dt to seconds
    double dt_seconds = dt_us.count() / 1e6;

    // Compute the derivative for each axis
    double dBx_dt = (magnetic_field_x - previous_magnetic_field_x) / dt_seconds;
    double dBy_dt = (magnetic_field_y - previous_magnetic_field_y) / dt_seconds;
    double dBz_dt = (magnetic_field_z - previous_magnetic_field_z) / dt_seconds;

    // Return the derivative vector
    return std::array<double, 3>{dBx_dt, dBy_dt, dBz_dt};
}

double BDot ::getMagnitude(std::array<double, 3> vector) {
    return std::sqrt(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);
}

void BDot ::updatePreviousReading(std::array<double, 3> magnetic_field, TimePoint reading) {
    this->m_previous_magnetic_field = magnetic_field;
    this->m_previous_magnetic_field_reading_time = reading;
}

bool BDot ::validateTimeDelta(std::chrono::microseconds dt_us) {
    // TODO(nateinaction): Take ODR in as an argument
    // Ensure magnetorquer readings are not faster than 100Hz magnetometer ODR
    if (std::chrono::milliseconds(10) > dt_us) {
        // Out of order readings or readings taken too quickly, cannot compute dipole moment
        errno = EINVAL;
        return false;
    }

    // TODO(nateinaction): Take this in as an argument
    // TODO(nateinaction): The 500ms limitation needs to be recorded in documentation, this prevents detumble from
    // working on slower rate groups Set previous magnetic field if last reading time was more than 500ms ago
    if (dt_us > std::chrono::milliseconds(500)) {
        // First reading or too much time has passed, cannot compute dipole moment
        errno = EAGAIN;
        return false;
    }

    return true;
}

}  // namespace Components
