// ======================================================================
// \title  DetumbleManager.cpp
// \brief  cpp file for DetumbleManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/DetumbleManager/DetumbleManager.hpp"

#include <Fw/Types/Assert.hpp>
#include <algorithm>
#include <cmath>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

DetumbleManager ::DetumbleManager(const char* const compName) : DetumbleManagerComponentBase(compName) {}

DetumbleManager ::~DetumbleManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void DetumbleManager ::run_handler(FwIndexType portNum, U32 context) {
    U32 currTime = this->getTime().getSeconds();

    // I'm not checking the value of isValid for paramGet calls since all of the parameters have a
    // default value, but maybe there should still be specific logging for if the default parameter
    // was used and retrieval failed.
    Fw::ParamValid isValid;

    if (this->detumbleRunning) {
        // Check if we've hit the max time detumble can run
        U32 maxTime = this->paramGet_MAX_TIME(isValid);
        if (currTime - this->startTime <= maxTime) {
            bool res = this->executeControlStep();
            if (!res) {
                // Log some error
            }

            this->iterations++;

            // Every 10 iterations (every second), check if the rotation per second is less than 12 degrees
            if (this->iterations % 10 == 0) {
                Drv::AngularVelocity angVel = this->angularVelocityGet_out(0);
                F64 rotationRate = this->getAngularVelocityMagnitude(angVel);

                F64 threshold = this->paramGet_ROTATIONAL_THRESHOLD(isValid);
                if (rotationRate < threshold) {
                    // Rotation is below threshold - can stop detumbling
                    this->detumbleRunning = false;
                    this->lastCompleted = currTime;
                }
            }
        } else {
            // Max time reached, disable detumble for now
            this->detumbleRunning = false;
            this->lastCompleted = currTime;
        }
    } else {
        // Check if the cooldown has ended, and start if so.
        if (currTime - this->lastCompleted >= this->paramGet_COOLDOWN(isValid)) {
            this->detumbleRunning = true;
            this->startTime = currTime;
        }
    }
}

bool DetumbleManager::executeControlStep() {
    Drv::MagneticField mgField = this->magneticFieldGet_out(0);
    if (this->prevMgField == this->EMPTY_MG_FIELD) {
        this->prevMgField = mgField;
    }

    Drv::DipoleMoment dpMoment = this->dipoleMomentGet_out(0, mgField, this->prevMgField);
    if (dpMoment == this->EMPTY_DP_MOMENT) {
        // Log some kinda error
        return false;
    }

    this->prevMgField = mgField;
    this->setDipoleMoment(dpMoment);

    return true;
}
void DetumbleManager::setDipoleMoment(Drv::DipoleMoment dpMoment) {
    // Convert dipole moment to (unlimited) current
    F64 unlimited_x = dpMoment.get_x() / (this->COIL_NUM_TURNS_X_Y * this->COIL_AREA_X_Y);
    F64 unlimited_y = dpMoment.get_y() / (this->COIL_NUM_TURNS_X_Y * this->COIL_AREA_X_Y);
    F64 unlimited_z = dpMoment.get_z() / (this->COIL_NUM_TURNS_Z * this->COIL_AREA_Z);

    // Limit current for each axis to max coil current
    F64 limited_x = std::fmin(std::fabs(unlimited_x), this->COIL_MAX_CURRENT_X_Y) * (unlimited_x >= 0 ? 1.0 : -1.0);
    F64 limited_y = std::fmin(std::fabs(unlimited_y), this->COIL_MAX_CURRENT_X_Y) * (unlimited_y >= 0 ? 1.0 : -1.0);
    F64 limited_z = std::fmin(std::fabs(unlimited_z), this->COIL_MAX_CURRENT_Z) * (unlimited_z >= 0 ? 1.0 : -1.0);

    F64 x1 = limited_x;
    F64 x2 = -limited_x;
    F64 y1 = limited_y;
    F64 y2 = -limited_y;
    F64 z1 = limited_z;

    this->magnetorquersSet_out(0, {x1, x2, y1, y2, z1});
}

F64 DetumbleManager::getAngularVelocityMagnitude(const Drv::AngularVelocity& angVel) {
    // Calculate magnitude in rad/s
    F64 magRadPerSec =
        std::sqrt(angVel.get_x() * angVel.get_x() + angVel.get_y() * angVel.get_y() + angVel.get_z() * angVel.get_z());

    // Convert rad/s to deg/s
    return magRadPerSec * 180.0 / this->PI;
}

}  // namespace Components
