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
