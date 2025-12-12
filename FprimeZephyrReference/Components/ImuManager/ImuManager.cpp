// ======================================================================
// \title  ImuManager.cpp
// \brief  cpp file for ImuManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/ImuManager/ImuManager.hpp"

#include <Fw/Types/Assert.hpp>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ImuManager ::ImuManager(const char* const compName) : ImuManagerComponentBase(compName) {}

ImuManager ::~ImuManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void ImuManager ::run_handler(FwIndexType portNum, U32 context) {
    Fw::Success condition;  // Ignoring for now

    Drv::Acceleration acceleration = this->accelerationGet_handler(0, condition);
    this->tlmWrite_Acceleration(acceleration);

    Drv::AngularVelocity angular_velocity = this->angularVelocityGet_handler(0, condition);
    this->tlmWrite_AngularVelocity(angular_velocity);

    Drv::MagneticField magnetic_field = this->magneticFieldGet_handler(0, condition);
    this->tlmWrite_MagneticField(magnetic_field);
}

Drv::Acceleration ImuManager ::accelerationGet_handler(FwIndexType portNum, Fw::Success& condition) {
    Drv::Acceleration acceleration = this->acceleration_out(portNum, condition);

    if (condition != Fw::Success::SUCCESS) {
        return acceleration;
    }

    double x = acceleration.get_x();
    double y = acceleration.get_y();
    double z = acceleration.get_z();

    this->applyAxisOrientation(&x, &y, &z);

    acceleration.set_x(x);
    acceleration.set_y(y);
    acceleration.set_z(z);

    return acceleration;
}

Drv::AngularVelocity ImuManager ::angularVelocityGet_handler(FwIndexType portNum, Fw::Success& condition) {
    Drv::AngularVelocity angular_velocity = this->angularVelocity_out(portNum, condition);

    if (condition != Fw::Success::SUCCESS) {
        return angular_velocity;
    }

    double x = angular_velocity.get_x();
    double y = angular_velocity.get_y();
    double z = angular_velocity.get_z();

    this->applyAxisOrientation(&x, &y, &z);

    angular_velocity.set_x(x);
    angular_velocity.set_y(y);
    angular_velocity.set_z(z);

    return angular_velocity;
}

Drv::MagneticField ImuManager ::magneticFieldGet_handler(FwIndexType portNum, Fw::Success& condition) {
    Drv::MagneticField magnetic_field = this->magneticField_out(portNum, condition);

    if (condition != Fw::Success::SUCCESS) {
        return magnetic_field;
    }

    double x = magnetic_field.get_x();
    double y = magnetic_field.get_y();
    double z = magnetic_field.get_z();

    this->applyAxisOrientation(&x, &y, &z);

    magnetic_field.set_x(x);
    magnetic_field.set_y(y);
    magnetic_field.set_z(z);

    return magnetic_field;
}

// ----------------------------------------------------------------------
//  Private helper methods
// ----------------------------------------------------------------------

void ImuManager ::applyAxisOrientation(double* x, double* y, double* z) {
    Fw::ParamValid valid;
    Components::AxisOrientation::T orientation = this->paramGet_AXIS_ORIENTATION(valid);

    this->tlmWrite_AxisOrientation(orientation);

    switch (orientation) {
        case Components::AxisOrientation::ROTATED_90_DEG_CW: {
            double temp_x = *x;
            *x = *y;
            *y = -temp_x;
            return;
        }
        case Components::AxisOrientation::ROTATED_90_DEG_CCW: {
            double temp_x = *x;
            *x = -*y;
            *y = temp_x;
            return;
        }
        case Components::AxisOrientation::ROTATED_180_DEG:
            *x = -*x;
            *y = -*y;
            return;
        default:
            return;
    }

    FW_ASSERT(0, static_cast<I32>(orientation));
}

}  // namespace Components
