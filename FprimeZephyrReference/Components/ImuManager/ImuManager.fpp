module Components {
    @ IMU Manager Component for F Prime FSW framework.
    passive component ImuManager {
        sync input port run: Svc.Sched

        @ Port for sending accelerationRead calls to the LSM6DSO Driver
        output port accelerationRead: Drv.AccelerationRead

        @ Port for sending angularVelocityRead calls to the LSM6DSO Driver
        output port angularVelocityRead: Drv.AngularVelocityRead

        @ Port for sending magneticFieldRead calls to the LIS2MDL Manager
        output port magneticFieldRead: Drv.MagneticFieldRead

        @ Port for sending temperatureRead calls to the LSM6DSO Driver
        output port temperatureRead: Drv.TemperatureRead

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller
    }
}
