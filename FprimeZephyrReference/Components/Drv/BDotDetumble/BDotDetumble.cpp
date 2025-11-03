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

Drv::DipoleMoment dipoleMomentGet_handler(const FwIndexType portNum,
                                          const Drv::MagneticField currMagField,
                                          const Drv::MagneticField prevMagField) {
    F32 magnitude = getMagnitude(currMagField);
    if (magnitude < 1e-6) {
        return;  // Figure out how I should handle the return here.
    }

    if (currMagField.timestamp <= prevMagField.timestamp) {
        return;  // Also figure out the return here.
    }

    F64* dB_dtArr = dB_dt(currMagField, prevMagField);
    if (dB_dtArr == nullptr) {
        return;  // Yeah just figure out how to properly raise error/return nothing.
    }

    F64 moment_x = this->gain * dB_dtArr[0] / magnitude;
    F64 moment_y = this->gain * dB_dtArr[1] / magnitude;
    F64 moment_z = this->gain * dB_dtArr[2] / magnitude;

    delete[] dB_dtArr;

    return Drv::DipoleMoment(moment_x, moment_y, moment_z);
}

F32 getMagnitude(const Drv::MagneticField magField) {
    return sqrt(f64_square(magField.x) + f64_square(magField.y) + f64_square(magField.z));
}

F64* dB_dt(const Drv::MagneticField currMagField, const Drv::MagneticField prevMagField) {
    I64 dt = currMagField.timestamp - prevMagField.timestamp;
    if (dt < 1e-6) {
        return nullptr;
    }

    F64 dBx_dt = (currMagField.x - prevMagField.x) / dt;
    F64 dBy_dt = (currMagField.y - prevMagField.y) / dt;
    F64 dBz_dt = (currMagField.z - prevMagField.z) / dt;

    F64* arr = new F64[3];
    arr[0] = dBx_dt;
    arr[1] = dBy_dt;
    arr[2] = dBz_dt;

    return arr;
}

}  // namespace Drv
