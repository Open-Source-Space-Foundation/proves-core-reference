
# Type definition
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
}

# Port definition
module Drv {
    port AccelerationRead -> Acceleration
    port AngularVelocityRead -> AngularVelocity
    port TemperatureRead -> F64
}

# Component definition
module Drv {
    @ LSM6DSO Driver Component for F Prime FSW framework.
    passive component Lsm6dsoDriver {
        @ Port to read the current acceleration in m/s^2.
        sync input port accelerationRead: AccelerationRead

        @ Port to read the current angular velocity in rad/s.
        sync input port angularVelocityRead: AngularVelocityRead

        @ Port to read the current temperature in degrees celsius.
        sync input port temperatureRead: TemperatureRead

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller
    }
}
