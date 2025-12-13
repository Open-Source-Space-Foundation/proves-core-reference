module Components {
    enum MagnetorquerCoilShape {
        RECTANGULAR,
        CIRCULAR,
    }
}

module Components {
    @ Detumble Manager Component for F Prime FSW framework.
    passive component DetumbleManager {

        ### Parameters ###

        @ Parameter for storing the rotational threshold for detumble to be enabled (default 12 deg/s)
        param ROTATIONAL_THRESHOLD: F32 default 12.0 id 1

        @ Parameter for storing the max amount of time an iteration can run
        param MAX_TIME: U32 default 10800 id 2

        @ Parameter for storing the cooldown after a detumble run finishes (in seconds)
        param COOLDOWN: U32 default 600 id 3

        ### Magnetorquer Properties Parameters ###

        # --- X+ Coil (Rectangular) ---
        param X_PLUS_ENABLED: bool default true id 4
        param X_PLUS_VOLTAGE: F64 default 3.3 id 9
        param X_PLUS_RESISTANCE: F64 default 57.2 id 10
        param X_PLUS_NUM_TURNS: F64 default 48.0 id 11
        param X_PLUS_LENGTH: F64 default 0.053 id 12
        param X_PLUS_WIDTH: F64 default 0.045 id 13
        param X_PLUS_SHAPE: MagnetorquerCoilShape default MagnetorquerCoilShape.RECTANGULAR id 33

        # --- X- Coil (Rectangular) ---
        param X_MINUS_ENABLED: bool default true id 5
        param X_MINUS_VOLTAGE: F64 default 3.3 id 14
        param X_MINUS_RESISTANCE: F64 default 57.2 id 15
        param X_MINUS_NUM_TURNS: F64 default 48.0 id 16
        param X_MINUS_LENGTH: F64 default 0.053 id 17
        param X_MINUS_WIDTH: F64 default 0.045 id 18
        param X_MINUS_SHAPE: MagnetorquerCoilShape default MagnetorquerCoilShape.RECTANGULAR id 34

        # --- Y+ Coil (Rectangular) ---
        param Y_PLUS_ENABLED: bool default true id 6
        param Y_PLUS_VOLTAGE: F64 default 3.3 id 19
        param Y_PLUS_RESISTANCE: F64 default 57.2 id 20
        param Y_PLUS_NUM_TURNS: F64 default 48.0 id 21
        param Y_PLUS_LENGTH: F64 default 0.053 id 22
        param Y_PLUS_WIDTH: F64 default 0.045 id 23
        param Y_PLUS_SHAPE: MagnetorquerCoilShape default MagnetorquerCoilShape.RECTANGULAR id 35

        # --- Y- Coil (Rectangular) ---
        param Y_MINUS_ENABLED: bool default true id 7
        param Y_MINUS_VOLTAGE: F64 default 3.3 id 24
        param Y_MINUS_RESISTANCE: F64 default 57.2 id 25
        param Y_MINUS_NUM_TURNS: F64 default 48.0 id 26
        param Y_MINUS_LENGTH: F64 default 0.053 id 27
        param Y_MINUS_WIDTH: F64 default 0.045 id 28
        param Y_MINUS_SHAPE: MagnetorquerCoilShape default MagnetorquerCoilShape.RECTANGULAR id 36

        # --- Z- Coil (Circular) ---
        param Z_MINUS_ENABLED: bool default true id 8
        param Z_MINUS_VOLTAGE: F64 default 3.3 id 29
        param Z_MINUS_RESISTANCE: F64 default 248.8 id 30
        param Z_MINUS_NUM_TURNS: F64 default 153.0 id 31
        param Z_MINUS_DIAMETER: F64 default 0.05755 id 32
        param Z_MINUS_SHAPE: MagnetorquerCoilShape default MagnetorquerCoilShape.CIRCULAR id 37

        ### Ports ###

        @ Run loop
        sync input port run: Svc.Sched

        @ Port for sending angularVelocityGet calls to the LSM6DSO Driver
        output port angularVelocityGet: AngularVelocityGet

        @ Port for sending magneticFieldGet calls to the LIS2MDL Manager
        output port magneticFieldGet: MagneticFieldGet

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

        event AngVelAmt(val: F64) severity warning low format "Angular velocity: {}"

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
