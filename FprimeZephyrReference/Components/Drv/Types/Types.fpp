# Type definitions for sensor readings data structures
module Drv {
    @ Acceleration reading in m/s^2
    struct Acceleration {
        x: F64 @< Acceleration in m/s^2 in the X direction.
        y: F64 @< Acceleration in m/s^2 in the Y direction.
        z: F64 @< Acceleration in m/s^2 in the Z direction.
    }

    @ Angular velocity reading in rad/s
    struct AngularVelocity {
        x: F64 @< Angular velocity in rad/s in the X direction.
        y: F64 @< Angular velocity in rad/s in the Y direction.
        z: F64 @< Angular velocity in rad/s in the Z direction.
    }

    @ Magnetic field in gauss.
    struct MagneticField {
        x: F64 @< Magnetic field in gauss in the X direction.
        y: F64 @< Magnetic field in gauss in the Y direction.
        z: F64 @< Magnetic field in gauss in the Z direction.
    }
}
