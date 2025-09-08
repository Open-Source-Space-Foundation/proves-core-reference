module Components {
    @ Component for F Prime FSW framework.
    passive component Imu {
        sync input port run: Svc.Sched

        struct MagneticField {
            x: F64
            y: F64
            z: F64
        }
        telemetry MagneticField: MagneticField

        struct Acceleration {
            x: F64
            y: F64
            z: F64
        }
        telemetry Acceleration: Acceleration

        struct AngularVelocity {
            x: F64
            y: F64
            z: F64
        }
        telemetry AngularVelocity: AngularVelocity

        telemetry Temperature: F64

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

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
