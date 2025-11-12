module Components {
    @ Detumble Manager Component for F Prime FSW framework.
    passive component DetumbleManager {

        @ Parameter for storing if detumble is currently running
        param DETUMBLE_RUNNING: bool default true \
            id 1

        @ Parameter for storing the rotational threshold for detumble to be enabled (default 12 deg/s)
        param ROTATIONAL_THRESHOLD: F32 default 12.0 \
            id 2

        @ Parameter for storing the max amount of time an iteration can run
        param MAX_TIME: U32 default 10800 \
            id 3

        @ Parameter for storing the cooldown after a detumble run finishes (in seconds)
        param COOLDOWN: U32 default 600 \
            id 4

        @ Parameter for storing the RTC time that a detumble run was completed/interrupted
        param LAST_COMPLETED: U32 default 0 \
            id 5

        param START_TIME: U32 default 0 \
            id 6

        @ Run loop
        sync input port run: Svc.Sched

        @ Port for getting the current time from the RTC
        output port timeGet: Drv.TimeGet

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

        @ Port for sending SetMagnetorquers calls to the MagnetorquerManager Component
        output port magnetorquersSet: Drv.SetMagnetorquers

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller
    }
}
