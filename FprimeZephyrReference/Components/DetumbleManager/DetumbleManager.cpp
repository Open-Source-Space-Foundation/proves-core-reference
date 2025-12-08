// ======================================================================
// \title  DetumbleManager.cpp
// \brief  cpp file for DetumbleManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/DetumbleManager/DetumbleManager.hpp"

#include <Fw/Types/Assert.hpp>
#include <Fw/Types/String.hpp>
#include <algorithm>
#include <cmath>
#include <string>

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
    // I'm not checking the value of isValid for paramGet calls since all of the parameters have a
    // default value, but maybe there should still be specific logging for if the default parameter
    // was used and retrieval failed.
    Fw::ParamValid isValid;

    if (this->bDotRunning) {
        // Give two iterations for the magnetorquers to fully turn off for magnetic reading
        if (this->m_itrCount > 2) {
            std::string reason = "";
            bool stepSuccess = this->executeControlStep(reason);

            if (!stepSuccess) {
                this->log_WARNING_HI_ControlStepFailed(Fw::String(reason.c_str()));
            }

            this->bDotRunning = false;
            this->m_itrCount = 0;
        } else {
            this->m_itrCount++;
        }

        return;
    }

    F64 angVelMagnitude = this->getAngularVelocityMagnitude(this->angularVelocityGet_out(0));
    if (angVelMagnitude < this->paramGet_ROTATIONAL_THRESHOLD(isValid)) {
        // Magnetude below threshold, disable magnetorquers
        bool values[5] = {false, false, false, false, false};
        this->setMagnetorquers(values);
    } else {
        this->bDotRunning = true;

        // Disable the magnetorquers so magnetic reading can take place
        bool values[5] = {false, false, false, false, false};
        this->setMagnetorquers(values);
    }
}

bool DetumbleManager::executeControlStep(std::string& reason) {
    Drv::MagneticField mgField = this->magneticFieldGet_out(0);
    if (this->prevMgField == this->EMPTY_MG_FIELD) {
        this->prevMgField = mgField;
    }

    Drv::DipoleMoment dpMoment = this->dipoleMomentGet_out(0, mgField, this->prevMgField);
    if (dpMoment == this->EMPTY_DP_MOMENT) {
        reason = "Dipole moment failed to calculate";
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

    // All true for now until we figure out how to determine what should be on or off
    bool values[5] = {true, true, true, true, true};
    this->setMagnetorquers(values);
}

F64 DetumbleManager::getAngularVelocityMagnitude(const Drv::AngularVelocity& angVel) {
    // Calculate magnitude in rad/s
    F64 magRadPerSec =
        std::sqrt(angVel.get_x() * angVel.get_x() + angVel.get_y() * angVel.get_y() + angVel.get_z() * angVel.get_z());

    // Convert rad/s to deg/s
    return magRadPerSec * 180.0 / this->PI;
}

void DetumbleManager::setMagnetorquers(bool val[5]) {
    for (int i = 0; i < 5; i++) {
        this->drv2605Toggle_out(i, val[i]);
    }
}

}  // namespace Components
