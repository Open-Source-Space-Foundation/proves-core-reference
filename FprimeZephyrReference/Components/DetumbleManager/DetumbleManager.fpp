module Components {
    @ Detumble Manager Component for F Prime FSW framework.
    passive component DetumbleManager {
        sync input port run: Svc.Sched

        @ Port for sending accelerationGet calls to the LSM6DSO Driver
        output port accelerationGet: Drv.AccelerationGet

        @ Port for sending angularVelocityGet calls to the LSM6DSO Driver
        output port angularVelocityGet: Drv.AngularVelocityGet

        @ Port for sending magneticFieldGet calls to the LIS2MDL Manager
        output port magneticFieldGet: Drv.MagneticFieldGet

        @ Port for sending temperatureGet calls to the LSM6DSO Driver
        output port temperatureGet: Drv.TemperatureGet

        @ Port for sending dipoleMomentGet calls to the BDotDetumble Component
        output port dipoleMomentGet: Drv.DipoleMomentGet

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller
    }
}
