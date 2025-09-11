module Components {
    @ IMU Component for F Prime FSW framework.
    passive component Imu {
        sync input port run: Svc.Sched

        @ Ports for sending calls to the LSM6DSO driver
        output port accelerationRead: Drv.AccelerationRead
        output port angularVelocityRead: Drv.AngularVelocityRead
        output port temperatureRead: Drv.TemperatureRead

        @ Port for sending calls to the LIS2MDL driver
        output port magneticFieldRead: Drv.MagneticFieldRead

        @ Telemetry channel for acceleration in m/s^2
        telemetry Acceleration: Drv.Acceleration

        @ Telemetry channel for angular velocity in rad/s
        telemetry AngularVelocity: Drv.AngularVelocity

        @ Telemetry channel for magnetic field in gauss
        telemetry MagneticField: Drv.MagneticField

        @ Telemetry channel for temperature in degrees Celsius
        telemetry Temperature: F64

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending telemetry channels to downlink
        telemetry port tlmOut
    }
}
