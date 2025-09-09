module Components {
    @ IMU Component for F Prime FSW framework.
    passive component Imu {
        sync input port run: Svc.Sched

        @ Acceleration reading in m/s^2
        struct Acceleration {
            x: F64
            y: F64
            z: F64
        }

        @ Telemetry channel for acceleration
        telemetry Acceleration: Acceleration

        @ Angular velocity reading in rad/s
        struct AngularVelocity {
            x: F64
            y: F64
            z: F64
        }

        @ Telemetry channel for angular velocity
        telemetry AngularVelocity: AngularVelocity

        @ Magnetic field reading in gauss
        struct MagneticField {
            x: F64
            y: F64
            z: F64
        }

        @ Telemetry channel for magnetic field
        telemetry MagneticField: MagneticField

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
