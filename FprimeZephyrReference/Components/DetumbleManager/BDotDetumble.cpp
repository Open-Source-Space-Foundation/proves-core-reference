// ======================================================================
// \title  BDotDetumble.cpp
// \brief  cpp file for BDotDetumble component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/BDotDetumble/BDotDetumble.hpp"

#include <cmath>

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

BDotDetumble ::BDotDetumble(const char* const compName) : BDotDetumbleComponentBase(compName) {}

BDotDetumble ::~BDotDetumble() {}

// ----------------------------------------------------------------------
//  public helper methods
// ----------------------------------------------------------------------

Drv::DipoleMoment BDotDetumble ::getDipoleMomentGet(Fw::Success& condition) {
    condition = Fw::Success::FAILURE;

    // Set previous magnetic field if uninitialized
    if (this->magneticFieldReadingTime(this->m_previousMagField) == Fw::Time()) {
        Fw::Success magFieldGetCondition;
        this->m_previousMagField = this->magneticFieldGet_out(0, magFieldGetCondition);
        return Drv::DipoleMoment();
    }

    // Get the current magnetic field reading from the magnetometer
    Fw::Success magFieldGetCondition;
    Drv::MagneticField currMagField = this->magneticFieldGet_out(0, magFieldGetCondition);

    // Compute magnitude of magnetic field
    F64 magnitude = this->getMagnitude(currMagField);
    if (magnitude < 1e-6) {
        return Drv::DipoleMoment();
    }

    // Compute dB/dt
    std::array<F64, 3> dB_dt = this->dB_dt(currMagField, this->m_previousMagField);

    // Get the gain from parameter
    Fw::ParamValid valid;
    F64 gain = this->paramGet_Gain(valid);  // not checking valid for now
    this->tlmWrite_Gain(gain);

    // Compute dipole moment m = -k * (dB/dt) / |B|
    F64 moment_x = gain * dB_dt[0] / magnitude;
    F64 moment_y = gain * dB_dt[1] / magnitude;
    F64 moment_z = gain * dB_dt[2] / magnitude;

    // Update previous magnetic field
    this->m_previousMagField = currMagField;

    // Return result
    condition = Fw::Success::SUCCESS;
    return Drv::DipoleMoment(moment_x, moment_y, moment_z);
}

// ----------------------------------------------------------------------
//  Private helper methods
// ----------------------------------------------------------------------

F64 BDotDetumble ::getMagnitude(const Drv::MagneticField magField) {
    return sqrt((magField.get_x() * magField.get_x()) + (magField.get_y() * magField.get_y()) +
                (magField.get_z() * magField.get_z()));
}

std::array<F64, 3> BDotDetumble ::dB_dt(const Drv::MagneticField currMagField, const Drv::MagneticField prevMagField) {
    // To compute dB/dt, we need the time difference between the two readings to be non-zero
    Fw::TimeInterval dt =
        Fw::TimeInterval(this->magneticFieldReadingTime(currMagField), this->magneticFieldReadingTime(prevMagField));
    if (dt.getSeconds() == 0 && dt.getUSeconds() == 0) {
        return std::array<F64, 3>{0.0, 0.0, 0.0};
    }

    // Compute the derivative for each axis
    F64 dt_seconds = static_cast<F64>(dt.getSeconds()) + static_cast<F64>(dt.getUSeconds()) / 1e6;
    F64 dBx_dt = (currMagField.get_x() - this->m_previousMagField.get_x()) / dt_seconds;
    F64 dBy_dt = (currMagField.get_y() - this->m_previousMagField.get_y()) / dt_seconds;
    F64 dBz_dt = (currMagField.get_z() - this->m_previousMagField.get_z()) / dt_seconds;

    // Return as array
    std::array<F64, 3> arr;
    arr[0] = dBx_dt;
    arr[1] = dBy_dt;
    arr[2] = dBz_dt;

    return arr;
}

Fw::Time BDotDetumble ::magneticFieldReadingTime(const Drv::MagneticField magField) {
    return Fw::Time(magField.get_timestamp().get_timeBase(), magField.get_timestamp().get_timeContext(),
                    magField.get_timestamp().get_seconds(), magField.get_timestamp().get_useconds());
}

}  // namespace Drv
