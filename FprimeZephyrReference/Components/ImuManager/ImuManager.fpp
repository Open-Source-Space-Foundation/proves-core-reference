module Components {
    @ IMU Manager Component for F Prime FSW framework.
    passive component ImuManager {
        sync input port run: Svc.Sched

        @ Port for sending accelerationGet calls to the LSM6DSO Driver
        output port accelerationGet: Drv.AccelerationGet

        @ Port for sending angularVelocityGet calls to the LSM6DSO Driver
        output port angularVelocityGet: Drv.AngularVelocityGet

        @ Port for sending magneticFieldGet calls to the LIS2MDL Manager
        output port magneticFieldGet: Drv.MagneticFieldGet

        @ Port for sending temperatureGet calls to the LSM6DSO Driver
        output port temperatureGet: Drv.TemperatureGet

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller
    }
}
