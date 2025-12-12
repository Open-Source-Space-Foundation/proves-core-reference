module Components {
    @ Axis orientation settings
    enum AxisOrientation {
        STANDARD @< Standard orientation
        ROTATED_90_DEG_CW @< Rotated 90 degrees clockwise
        ROTATED_90_DEG_CCW @< Rotated 90 degrees counter-clockwise
        ROTATED_180_DEG @< Rotated 180 degrees
    }
}

module Components {
    @ IMU Manager Component for F Prime FSW framework.
    passive component ImuManager {

        ### Ports ###

        @ Port to trigger periodic data fetching and telemetry updating
        sync input port run: Svc.Sched

        @ Port to read the current acceleration in m/s^2.
        sync input port accelerationGet: Drv.AccelerationGet

        @ Port to read the current angular velocity in rad/s.
        sync input port angularVelocityGet: Drv.AngularVelocityGet

        @ Port to read the current magnetic field in gauss.
        sync input port magneticFieldGet: Drv.MagneticFieldGet

        @ Port for sending accelerationGet calls to the LSM6DSO Driver
        output port acceleration: Drv.AccelerationGet

        @ Port for sending angularVelocityGet calls to the LSM6DSO Driver
        output port angularVelocity: Drv.AngularVelocityGet

        @ Port for sending magneticFieldGet calls to the LIS2MDL Manager
        output port magneticField: Drv.MagneticFieldGet

        ### Parameters ###

        @ Parameter for storing the axis orientation
        param AXIS_ORIENTATION: AxisOrientation default AxisOrientation.STANDARD id 1

        ### Telemetry channels ###

        @ Telemetry channel for axis orientation
        telemetry AxisOrientation: AxisOrientation

        @ Telemetry channel for current acceleration in m/s^2.
        telemetry Acceleration: Drv.Acceleration

        @ Telemetry channel for current angular velocity in rad/s.
        telemetry AngularVelocity: Drv.AngularVelocity

        @ Telemetry channel for current magnetic field in gauss.
        telemetry MagneticField: Drv.MagneticField

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending command registrations
        command reg port cmdRegOut

        @ Port for receiving commands
        command recv port cmdIn

        @ Port for sending command responses
        command resp port cmdResponseOut

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

        @ Port for sending telemetry channels to downlink
        telemetry port tlmOut

        @ Port to return the value of a parameter
        param get port prmGetOut

        @Port to set the value of a parameter
        param set port prmSetOut

    }
}
