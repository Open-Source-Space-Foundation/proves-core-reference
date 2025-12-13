// ======================================================================
// \title  DetumbleManager.cpp
// \brief  cpp file for DetumbleManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/DetumbleManager/DetumbleManager.hpp"

#include <Fw/Types/String.hpp>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

DetumbleManager ::DetumbleManager(const char* const compName) : DetumbleManagerComponentBase(compName) {
    Fw::ParamValid isValid;

    // Initialize coil parameters from configuration parameters
    // X+ Coil
    this->m_xPlusMagnetorquer.enabled = this->paramGet_X_PLUS_ENABLED(isValid);
    this->m_xPlusMagnetorquer.voltage = this->paramGet_X_PLUS_VOLTAGE(isValid);
    this->m_xPlusMagnetorquer.resistance = this->paramGet_X_PLUS_RESISTANCE(isValid);
    this->m_xPlusMagnetorquer.numTurns = this->paramGet_X_PLUS_NUM_TURNS(isValid);
    this->m_xPlusMagnetorquer.shape = magnetorquerCoil::RECTANGULAR;
    this->m_xPlusMagnetorquer.width = this->paramGet_X_PLUS_WIDTH(isValid);
    this->m_xPlusMagnetorquer.length = this->paramGet_X_PLUS_LENGTH(isValid);

    // X- Coil
    this->m_xMinusMagnetorquer.enabled = this->paramGet_X_MINUS_ENABLED(isValid);
    this->m_xMinusMagnetorquer.voltage = this->paramGet_X_MINUS_VOLTAGE(isValid);
    this->m_xMinusMagnetorquer.resistance = this->paramGet_X_MINUS_RESISTANCE(isValid);
    this->m_xMinusMagnetorquer.numTurns = this->paramGet_X_MINUS_NUM_TURNS(isValid);
    this->m_xMinusMagnetorquer.shape = magnetorquerCoil::RECTANGULAR;
    this->m_xMinusMagnetorquer.width = this->paramGet_X_MINUS_WIDTH(isValid);
    this->m_xMinusMagnetorquer.length = this->paramGet_X_MINUS_LENGTH(isValid);

    // Y+ Coil
    this->m_yPlusMagnetorquer.enabled = this->paramGet_Y_PLUS_ENABLED(isValid);
    this->m_yPlusMagnetorquer.voltage = this->paramGet_Y_PLUS_VOLTAGE(isValid);
    this->m_yPlusMagnetorquer.resistance = this->paramGet_Y_PLUS_RESISTANCE(isValid);
    this->m_yPlusMagnetorquer.numTurns = this->paramGet_Y_PLUS_NUM_TURNS(isValid);
    this->m_yPlusMagnetorquer.shape = magnetorquerCoil::RECTANGULAR;
    this->m_yPlusMagnetorquer.width = this->paramGet_Y_PLUS_WIDTH(isValid);
    this->m_yPlusMagnetorquer.length = this->paramGet_Y_PLUS_LENGTH(isValid);

    // Y- Coil
    this->m_yMinusMagnetorquer.enabled = this->paramGet_Y_MINUS_ENABLED(isValid);
    this->m_yMinusMagnetorquer.voltage = this->paramGet_Y_MINUS_VOLTAGE(isValid);
    this->m_yMinusMagnetorquer.resistance = this->paramGet_Y_MINUS_RESISTANCE(isValid);
    this->m_yMinusMagnetorquer.numTurns = this->paramGet_Y_MINUS_NUM_TURNS(isValid);
    this->m_yMinusMagnetorquer.shape = magnetorquerCoil::RECTANGULAR;
    this->m_yMinusMagnetorquer.width = this->paramGet_Y_MINUS_WIDTH(isValid);
    this->m_yMinusMagnetorquer.length = this->paramGet_Y_MINUS_LENGTH(isValid);

    // Z- Coil
    this->m_zMinusMagnetorquer.enabled = this->paramGet_Z_MINUS_ENABLED(isValid);
    this->m_zMinusMagnetorquer.voltage = this->paramGet_Z_MINUS_VOLTAGE(isValid);
    this->m_zMinusMagnetorquer.resistance = this->paramGet_Z_MINUS_RESISTANCE(isValid);
    this->m_zMinusMagnetorquer.numTurns = this->paramGet_Z_MINUS_NUM_TURNS(isValid);
    this->m_zMinusMagnetorquer.shape = magnetorquerCoil::CIRCULAR;
    this->m_zMinusMagnetorquer.diameter = this->paramGet_Z_MINUS_DIAMETER(isValid);
}

DetumbleManager ::~DetumbleManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void DetumbleManager ::run_handler(FwIndexType portNum, U32 context) {
    // I'm not checking the value of isValid for paramGet calls since all of the parameters have a
    // default value, but maybe there should still be specific logging for if the default parameter
    // was used and retrieval failed.
    Fw::ParamValid isValid;

    if (this->m_bDotRunning) {
        // Give two iterations for the magnetorquers to fully turn off for magnetic reading
        if (this->m_itrCount > 2) {
            std::string reason = "";
            bool stepSuccess = this->executeControlStep(reason);

            if (!stepSuccess) {
                this->log_WARNING_HI_ControlStepFailed(Fw::String(reason.c_str()));
            }

            this->m_bDotRunning = false;
            this->m_itrCount = 0;
        } else {
            this->m_itrCount++;
        }

        return;
    }

    Fw::Success condition;
    F64 angVelMagnitude = this->getAngularVelocityMagnitude(this->angularVelocityGet_out(0, condition));
    if (angVelMagnitude < this->paramGet_ROTATIONAL_THRESHOLD(isValid)) {
        // Magnetude below threshold, disable magnetorquers
        bool values[5] = {false, false, false, false, false};
        this->setMagnetorquers(values);
    } else {
        this->m_bDotRunning = true;

        // Disable the magnetorquers so magnetic reading can take place
        bool values[5] = {false, false, false, false, false};
        this->setMagnetorquers(values);
    }
}

// ----------------------------------------------------------------------
//  Private helper methods
// ----------------------------------------------------------------------

bool DetumbleManager ::executeControlStep(std::string& reason) {
    Fw::Success condition;

    Drv::DipoleMoment dpMoment = this->dipoleMomentGet_out(0, condition);
    if (condition != Fw::Success::SUCCESS) {
        reason = "Dipole moment retrieval failed";
        return false;
    }

    this->setDipoleMoment(dpMoment);

    return true;
}

void DetumbleManager ::setDipoleMoment(Drv::DipoleMoment dpMoment) {
    // Convert dipole moment to (unlimited) current
    F64 unlimited_x1 =
        dpMoment.get_x() / (this->m_xPlusMagnetorquer.numTurns * this->getCoilArea(this->m_xPlusMagnetorquer));
    F64 unlimited_x2 =
        dpMoment.get_x() / (this->m_xMinusMagnetorquer.numTurns * this->getCoilArea(this->m_xMinusMagnetorquer));
    F64 unlimited_y1 =
        dpMoment.get_y() / (this->m_yPlusMagnetorquer.numTurns * this->getCoilArea(this->m_yPlusMagnetorquer));
    F64 unlimited_y2 =
        dpMoment.get_y() / (this->m_yMinusMagnetorquer.numTurns * this->getCoilArea(this->m_yMinusMagnetorquer));
    F64 unlimited_z =
        dpMoment.get_z() / (this->m_zMinusMagnetorquer.numTurns * this->getCoilArea(this->m_zMinusMagnetorquer));

    // Limit current for each axis to max coil current
    F64 limited_x1 = std::fmin(std::fabs(unlimited_x1), this->getMaxCoilCurrent(this->m_xPlusMagnetorquer)) *
                     (unlimited_x1 >= 0 ? 1.0 : -1.0);
    F64 limited_x2 = std::fmin(std::fabs(unlimited_x2), this->getMaxCoilCurrent(this->m_xMinusMagnetorquer)) *
                     (unlimited_x2 >= 0 ? 1.0 : -1.0);
    F64 limited_y1 = std::fmin(std::fabs(unlimited_y1), this->getMaxCoilCurrent(this->m_yPlusMagnetorquer)) *
                     (unlimited_y1 >= 0 ? 1.0 : -1.0);
    F64 limited_y2 = std::fmin(std::fabs(unlimited_y2), this->getMaxCoilCurrent(this->m_yMinusMagnetorquer)) *
                     (unlimited_y2 >= 0 ? 1.0 : -1.0);
    F64 limited_z = std::fmin(std::fabs(unlimited_z), this->getMaxCoilCurrent(this->m_yMinusMagnetorquer)) *
                    (unlimited_z >= 0 ? 1.0 : -1.0);

    F64 x1 = limited_x1;
    F64 x2 = -limited_x2;
    F64 y1 = limited_y1;
    F64 y2 = -limited_y2;
    F64 z1 = limited_z;

    // All true for now until we figure out how to determine what should be on or off
    bool values[5] = {true, true, true, true, true};
    this->setMagnetorquers(values);
}

F64 DetumbleManager ::getAngularVelocityMagnitude(const Drv::AngularVelocity& angVel) {
    // Calculate magnitude in rad/s
    F64 magRadPerSec =
        std::sqrt(angVel.get_x() * angVel.get_x() + angVel.get_y() * angVel.get_y() + angVel.get_z() * angVel.get_z());

    return magRadPerSec * 180.0 / this->PI;
}

void DetumbleManager ::setMagnetorquers(bool val[5]) {
    this->xPlusToggle_out(0, val[0]);
    this->xMinusToggle_out(0, val[1]);
    this->yPlusToggle_out(0, val[2]);
    this->yMinusToggle_out(0, val[3]);
    this->zMinusToggle_out(0, val[4]);
}

F64 DetumbleManager ::getCoilArea(const magnetorquerCoil& coil) {
    if (coil.shape == magnetorquerCoil::CIRCULAR) {
        return this->PI * std::pow(coil.diameter / 2.0, 2.0);
    }
    // Default to Rectangular
    return coil.width * coil.length;
}

F64 DetumbleManager ::getMaxCoilCurrent(const magnetorquerCoil& coil) {
    // Ohm's law: I = V / R
    if (coil.resistance == 0.0) {
        return 0.0;
    }
    return coil.voltage / coil.resistance;
}

}  // namespace Components
