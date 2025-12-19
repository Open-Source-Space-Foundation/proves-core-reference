module Components {
    enum MagnetorquerCoilShape {
        RECTANGULAR, @< Rectangular coil shape
        CIRCULAR,    @< Circular coil shape
    }

    enum DetumbleMode {
        DISABLED, @< Manual override, coils forced off
        AUTO,     @< Automatic detumbling based on angular velocity
    }

    enum DetumbleState {
        COOLDOWN, @< Waiting for magnetometers to settle
        SENSING,  @< Reading magnetic field
        TORQUING, @< Applying torque
    }
}

module Components {
    @ Detumble Manager Component for F Prime FSW framework.
    passive component DetumbleManager {

        ### Parameters ###

        @ Parameter for storing the operating mode (default DISABLED)
        param OPERATING_MODE: DetumbleMode default DetumbleMode.DISABLED id 0

        @ Parameter for storing the rotational threshold for detumble to be enabled (default 12 deg/s)
        param ROTATIONAL_THRESHOLD: F64 default 12.0 id 1

        @ Parameter for storing the cooldown duration
        param COOLDOWN_DURATION: Fw.TimeIntervalValue default {seconds = 1, useconds = 0} id 3

        @ Parameter for storing the detumble torquing duration
        param TORQUE_DURATION: Fw.TimeIntervalValue default {seconds = 3, useconds = 0} id 38

        ### Magnetorquer Properties Parameters ###

        # --- X+ Coil ---
        param X_PLUS_ENABLED: bool default true id 4
        param X_PLUS_VOLTAGE: F64 default 3.3 id 9
        param X_PLUS_RESISTANCE: F64 default 57.2 id 10
        param X_PLUS_NUM_TURNS: F64 default 48.0 id 11
        param X_PLUS_LENGTH: F64 default 0.053 id 12
        param X_PLUS_WIDTH: F64 default 0.045 id 13
        param X_PLUS_SHAPE: MagnetorquerCoilShape default MagnetorquerCoilShape.RECTANGULAR id 33

        # --- X- Coil ---
        param X_MINUS_ENABLED: bool default true id 5
        param X_MINUS_VOLTAGE: F64 default 3.3 id 14
        param X_MINUS_RESISTANCE: F64 default 57.2 id 15
        param X_MINUS_NUM_TURNS: F64 default 48.0 id 16
        param X_MINUS_LENGTH: F64 default 0.053 id 17
        param X_MINUS_WIDTH: F64 default 0.045 id 18
        param X_MINUS_SHAPE: MagnetorquerCoilShape default MagnetorquerCoilShape.RECTANGULAR id 34

        # --- Y+ Coil ---
        param Y_PLUS_ENABLED: bool default true id 6
        param Y_PLUS_VOLTAGE: F64 default 3.3 id 19
        param Y_PLUS_RESISTANCE: F64 default 57.2 id 20
        param Y_PLUS_NUM_TURNS: F64 default 48.0 id 21
        param Y_PLUS_LENGTH: F64 default 0.053 id 22
        param Y_PLUS_WIDTH: F64 default 0.045 id 23
        param Y_PLUS_SHAPE: MagnetorquerCoilShape default MagnetorquerCoilShape.RECTANGULAR id 35

        # --- Y- Coil ---
        param Y_MINUS_ENABLED: bool default true id 7
        param Y_MINUS_VOLTAGE: F64 default 3.3 id 24
        param Y_MINUS_RESISTANCE: F64 default 57.2 id 25
        param Y_MINUS_NUM_TURNS: F64 default 48.0 id 26
        param Y_MINUS_LENGTH: F64 default 0.053 id 27
        param Y_MINUS_WIDTH: F64 default 0.045 id 28
        param Y_MINUS_SHAPE: MagnetorquerCoilShape default MagnetorquerCoilShape.RECTANGULAR id 36

        # --- Z- Coil ---
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
        output port xPlusStart: Drv.StartMagnetorquer

        @ Port for stopping the X+ magnetorquer
        output port xPlusStop: Fw.Signal

        @ Port for triggering the X- magnetorquer
        output port xMinusStart: Drv.StartMagnetorquer

        @ Port for stopping the X- magnetorquer
        output port xMinusStop: Fw.Signal

        @ Port for triggering the Y+ magnetorquer
        output port yPlusStart: Drv.StartMagnetorquer

        @ Port for stopping the Y+ magnetorquer
        output port yPlusStop: Fw.Signal

        @ Port for triggering the Y- magnetorquer
        output port yMinusStart: Drv.StartMagnetorquer

        @ Port for stopping the Y- magnetorquer
        output port yMinusStop: Fw.Signal

        @ Port for triggering the Z- magnetorquer
        output port zMinusStart: Drv.StartMagnetorquer

        @ Port for stopping the Z- magnetorquer
        output port zMinusStop: Fw.Signal

        ### Events ###

        @ Event for reporting dipole moment retrieval failure
        event DipoleMomentRetrievalFailed() severity warning low format "Failed to retrieve dipole moment." throttle 5

        @ Event for reporting angular velocity retrieval failure
        event AngularVelocityRetrievalFailed() severity warning low format "Failed to retrieve angular velocity." throttle 5

        ### Telemetry ###

        @ Current operating mode
        telemetry Mode: DetumbleMode

        @ Current internal state
        telemetry State: DetumbleState

        @ Rotational velocity below threshold
        telemetry BelowRotationalThreshold: bool

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
