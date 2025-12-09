// ======================================================================
// \title  BDotDetumble.cpp
// \author aychar
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

Drv::DipoleMoment BDotDetumble::dipoleMomentGet_handler(const FwIndexType portNum,
                                                        const Drv::MagneticField& currMagField,
                                                        const Drv::MagneticField& prevMagField) {
    F64 magnitude = this->getMagnitude(currMagField);
    if (magnitude < 1e-6) {
        return Drv::DipoleMoment();
    }

    if (this->magneticFieldReadingTime(currMagField) <= this->magneticFieldReadingTime(prevMagField)) {
        return Drv::DipoleMoment();
    }

    std::array<F64, 3> dB_dtArr = this->dB_dt(currMagField, prevMagField);

    F64 moment_x = this->m_gain * dB_dtArr[0] / magnitude;
    F64 moment_y = this->m_gain * dB_dtArr[1] / magnitude;
    F64 moment_z = this->m_gain * dB_dtArr[2] / magnitude;

    return Drv::DipoleMoment(moment_x, moment_y, moment_z);
}

F64 BDotDetumble::getMagnitude(const Drv::MagneticField magField) {
    return sqrt((magField.get_x() * magField.get_x()) + (magField.get_y() * magField.get_y()) +
                (magField.get_z() * magField.get_z()));
}

std::array<F64, 3> BDotDetumble::dB_dt(const Drv::MagneticField currMagField, const Drv::MagneticField prevMagField) {
    // To compute dB/dt, we need the time difference between the two readings to be non-zero
    Fw::TimeInterval dt =
        Fw::TimeInterval(this->magneticFieldReadingTime(currMagField), this->magneticFieldReadingTime(prevMagField));
    if (dt.getSeconds() == 0 && dt.getUSeconds() == 0) {
        return std::array<F64, 3>{0.0, 0.0, 0.0};
    }

    F64 dt_seconds = static_cast<F64>(dt.getSeconds()) + static_cast<F64>(dt.getUSeconds()) / 1e6;
    F64 dBx_dt = (currMagField.get_x() - prevMagField.get_x()) / dt_seconds;
    F64 dBy_dt = (currMagField.get_y() - prevMagField.get_y()) / dt_seconds;
    F64 dBz_dt = (currMagField.get_z() - prevMagField.get_z()) / dt_seconds;

    std::array<F64, 3> arr;
    arr[0] = dBx_dt;
    arr[1] = dBy_dt;
    arr[2] = dBz_dt;

    return arr;
}

Fw::Time BDotDetumble::magneticFieldReadingTime(const Drv::MagneticField magField) {
    return Fw::Time(magField.get_timestamp().get_timeBase(), magField.get_timestamp().get_timeContext(),
                    magField.get_timestamp().get_seconds(), magField.get_timestamp().get_useconds());
}

}  // namespace Drv
