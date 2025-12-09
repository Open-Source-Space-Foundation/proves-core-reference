module Components {
    @ Detumble Manager Component for F Prime FSW framework.
    passive component DetumbleManager {

        ### Parameters ###

        @ Parameter for storing the rotational threshold for detumble to be enabled (default 12 deg/s)
        param ROTATIONAL_THRESHOLD: F32 default 12.0 \
            id 1

        @ Parameter for storing the max amount of time an iteration can run
        param MAX_TIME: U32 default 10800 \
            id 2

        @ Parameter for storing the cooldown after a detumble run finishes (in seconds)
        param COOLDOWN: U32 default 600 \
            id 3

        ### Ports ###

        @ Run loop
        sync input port run: Svc.Sched

        @ Port for sending angularVelocityGet calls to the LSM6DSO Driver
        output port angularVelocityGet: Drv.AngularVelocityGet

        @ Port for sending magneticFieldGet calls to the LIS2MDL Manager
        output port magneticFieldGet: Drv.MagneticFieldGet

        @ Port for sending dipoleMomentGet calls to the BDotDetumble Component
        output port dipoleMomentGet: Drv.DipoleMomentGet

        @ Port for triggering the X+ magnetorquer
        output port xPlusToggle: Drv.MagnetorquerToggle

        @ Port for triggering the X- magnetorquer
        output port xMinusToggle: Drv.MagnetorquerToggle

        @ Port for triggering the Y+ magnetorquer
        output port yPlusToggle: Drv.MagnetorquerToggle

        @ Port for triggering the Y- magnetorquer
        output port yMinusToggle: Drv.MagnetorquerToggle

        @ Port for triggering the Z- magnetorquer
        output port zMinusToggle: Drv.MagnetorquerToggle

        ### Events ###

        @ Event for reporting that a detumble control step run failed
        event ControlStepFailed(reason: string) severity warning high format "Control step failed for reason: {}"

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

        @ Port for getting parameters
        param get port prmGetOut

        @ Port for setting parameters
        param set port prmSetOut

        @ Port for sending command registrations
        command reg port cmdRegOut

        @ Port for receiving commands
        command recv port cmdIn

        @ Port for sending command responses
        command resp port cmdResponseOut

    }
}
