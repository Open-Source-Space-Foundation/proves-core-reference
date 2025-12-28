// ======================================================================
// \title  Magnetorquer.cpp
// \brief  cpp file for Magnetorquer implementation class
// ======================================================================

#include "Magnetorquer.hpp"

#include <algorithm>
#include <cmath>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Magnetorquer ::Magnetorquer() {}

Magnetorquer ::~Magnetorquer() {}

// ----------------------------------------------------------------------
//  Public helper methods
// ----------------------------------------------------------------------

std::int8_t Magnetorquer ::dipoleMomentToCurrent(double dipole_moment_component) {
    // Calculate target current
    double target_current = this->computeTargetCurrent(dipole_moment_component);

    // Clamp current and scale to int8_t range
    std::int8_t clampedCurrent = this->computeClampedCurrent(target_current);

    // Adjust sign so that a positive drive level always produces the same dipole / torque
    // direction, compensating for the opposite physical orientation of the "minus" coils.
    return this->m_direction_sign * clampedCurrent;
}

// ----------------------------------------------------------------------
//  Private helper methods
// ----------------------------------------------------------------------

double Magnetorquer ::getCoilArea() {
    // Calculate area based on coil shape
    if (this->m_shape == CoilShape::CIRCULAR) {
        return this->PI * std::pow(this->m_diameter / 2.0, 2.0);
    }

    // Default to Rectangular
    return this->m_width * this->m_length;
}

double Magnetorquer ::computeTargetCurrent(double dipole_moment_component) {
    // Get coil area
    double area = this->getCoilArea();

    // Avoid division by zero
    if (this->m_turns == 0.0 || area == 0.0) {
        return 0.0;
    }

    // Calculate target current
    return dipole_moment_component / (this->m_turns * area);
}

double Magnetorquer ::getMaxCoilCurrent() {
    // Avoid division by zero
    if (this->m_resistance == 0.0) {
        return 0.0;
    }

    // Calculate maximum current
    return this->m_voltage / this->m_resistance;
}

std::int8_t Magnetorquer ::computeClampedCurrent(double target_current) {
    double clampedCurrent;

    // Get maximum coil current
    double max_current = this->getMaxCoilCurrent();

    // Clamp to max current
    if (std::fabs(target_current) > max_current) {
        clampedCurrent = (target_current > 0) ? max_current : -max_current;
    } else {
        clampedCurrent = target_current;
    }

    // Avoid division by zero
    if (max_current == 0.0) {
        return 0;
    }

    // Scale to int8_t range [-127, 127]
    return static_cast<std::int8_t>(std::round((clampedCurrent / max_current) * 127.0));
}

}  // namespace Components
