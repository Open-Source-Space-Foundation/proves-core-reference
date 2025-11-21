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

    if (currMagField.get_timestamp() <= prevMagField.get_timestamp()) {
        return Drv::DipoleMoment();
    }

    F64* dB_dtArr = this->dB_dt(currMagField, prevMagField);
    if (dB_dtArr == nullptr) {
        return Drv::DipoleMoment();
    }

    F64 moment_x = this->gain * dB_dtArr[0] / magnitude;
    F64 moment_y = this->gain * dB_dtArr[1] / magnitude;
    F64 moment_z = this->gain * dB_dtArr[2] / magnitude;

    delete[] dB_dtArr;

    return Drv::DipoleMoment(moment_x, moment_y, moment_z);
}

F64 BDotDetumble::getMagnitude(const Drv::MagneticField magField) {
    return sqrt((magField.get_x() * magField.get_x()) + (magField.get_y() * magField.get_y()) +
                (magField.get_z() * magField.get_z()));
}

F64* BDotDetumble::dB_dt(const Drv::MagneticField currMagField, const Drv::MagneticField prevMagField) {
    I64 dt = currMagField.get_timestamp() - prevMagField.get_timestamp();
    if (dt < 1e-6) {
        return nullptr;
    }

    F64 dBx_dt = (currMagField.get_x() - prevMagField.get_x()) / dt;
    F64 dBy_dt = (currMagField.get_y() - prevMagField.get_y()) / dt;
    F64 dBz_dt = (currMagField.get_z() - prevMagField.get_z()) / dt;

    F64* arr = new F64[3];
    arr[0] = dBx_dt;
    arr[1] = dBy_dt;
    arr[2] = dBz_dt;

    return arr;
}

}  // namespace Drv
