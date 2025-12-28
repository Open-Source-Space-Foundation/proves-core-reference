// ======================================================================
// \title  StrategySelector.cpp
// \brief  cpp file for StrategySelector implementation class
// ======================================================================

#include "StrategySelector.hpp"

#include <cmath>

namespace Components {
// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

StrategySelector ::StrategySelector() {}

StrategySelector ::~StrategySelector() {}

// ----------------------------------------------------------------------
//  Public helper methods
// ----------------------------------------------------------------------

StrategySelector::Strategy StrategySelector ::fromAngularVelocity(std::array<double, 3> angular_velocity) {
    // Convert angular velocity to deg/s
    double angular_velocity_deg_sec = this->getAngularVelocityMagnitude(angular_velocity);

    // If below lower deadband threshold, don't detumble
    if (angular_velocity_deg_sec < this->m_deadband_lower_threshold) {
        // Set target rotational threshold to upper deadband
        this->m_rotation_target = this->m_deadband_upper_threshold;

        // Return decision
        return IDLE;
    }

    // If above B-Dot maximum threshold, use hysteresis detumble
    if (angular_velocity_deg_sec >= this->m_bdot_max_threshold) {
        // Return decision
        return HYSTERESIS;
    }

    // If within B-Dot effective range and above target, use B-Dot detumble
    if (angular_velocity_deg_sec >= this->m_rotation_target) {
        // Set target rotational threshold to lower deadband
        this->m_rotation_target = this->m_deadband_lower_threshold;

        // Return decision
        return BDOT;
    }

    // We are within the deadband hold state, don't detumble
    return IDLE;
}

void StrategySelector ::configure(double bdot_max_threshold,
                                  double deadband_upper_threshold,
                                  double deadband_lower_threshold) {
    this->m_bdot_max_threshold = bdot_max_threshold;
    this->m_deadband_upper_threshold = deadband_upper_threshold;
    this->m_deadband_lower_threshold = deadband_lower_threshold;

    // Set rotation target to lower deadband if not already set to either threshold
    if (this->m_rotation_target != deadband_upper_threshold || this->m_rotation_target != deadband_lower_threshold) {
        this->m_rotation_target = deadband_lower_threshold;
    }
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

double StrategySelector ::getAngularVelocityMagnitude(std::array<double, 3> angular_velocity) {
    // Compute magnitude in rad/s
    double angular_velocity_magnitude_rad_sec =
        std::sqrt(angular_velocity[0] * angular_velocity[0] + angular_velocity[1] * angular_velocity[1] +
                  angular_velocity[2] * angular_velocity[2]);

    // Convert to deg/s
    double angular_velocity_magnitude_deg_sec = angular_velocity_magnitude_rad_sec * 180.0 / this->PI;
    return angular_velocity_magnitude_deg_sec;
}

}  // namespace Components
