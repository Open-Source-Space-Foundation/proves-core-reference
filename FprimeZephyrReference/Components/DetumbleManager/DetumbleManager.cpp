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
    U32 currTime = this->timeGet_out(0);

    if (this->paramGet_DETUMBLE_RUNNING()) {
        // Check if we've hit the max time detumble can run
        if (currTime - this->paramGet_START_TIME() <= this->paramGet_MAX_TIME()) {
            bool res = this->executeControlStep();
            if (!res) {
                // Log some error
                continue;
            }
        } else {
            // Max iterations reached, disable detumble for now
            this->paramSet_DETUMBLE_RUNNING(false);
            this->paramSet_LAST_COMPLETED(currTime);
        }
    } else {
        // Check if the cooldown has ended, and start if so.
        if (currTime - this->paramGet_LAST_COMPLETED() >= this->paramGet_COOLDOWN()) {
            this->paramSet_DETUMBLE_RUNNING(true);
            this->paramSet_START_TIME(currTime);
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
    F64 limited_x = std::min(std::fabs(unlimited_x), this->COIL_MAX_CURRENT_X_Y) * (unlimited_x >= 0 ? 1.0f : -1.0f);
    F64 limited_y = std::min(std::fabs(unlimited_y), this->COIL_MAX_CURRENT_X_Y) * (unlimited_y >= 0 ? 1.0f : -1.0f);
    F64 limited_z = std::min(std::fabs(unlimited_z), this->COIL_MAX_CURRENT_Z) * (unlimited_z >= 0 ? 1.0f : -1.0f);

    F64 x1 = limited_x;
    F64 x2 = -limited_x;
    F64 y1 = limited_y;
    F64 y2 = -limited_y;
    F64 z1 = limited_z;

    this->magnetorquersSet_out(0, {x1, x2, y1, y2, z1});
}

}  // namespace Components
