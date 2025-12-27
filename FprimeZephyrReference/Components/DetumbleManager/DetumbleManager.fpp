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
        param COOLDOWN_DURATION: Fw.TimeIntervalValue default {seconds = 0, useconds = 20000} id 3

        @ Parameter for storing the detumble torquing duration
        param TORQUE_DURATION: Fw.TimeIntervalValue default {seconds = 0, useconds = 20000} id 38

        ### Magnetorquer Properties Parameters ###

        # --- X+ Coil ---
        param X_PLUS_VOLTAGE: F64 default 3.3 id 9
        param X_PLUS_RESISTANCE: F64 default 13 id 10
        param X_PLUS_NUM_TURNS: F64 default 48.0 id 11
        param X_PLUS_LENGTH: F64 default 0.053 id 12
        param X_PLUS_WIDTH: F64 default 0.045 id 13
        param X_PLUS_SHAPE: MagnetorquerCoilShape default MagnetorquerCoilShape.RECTANGULAR id 33

        # --- X- Coil ---
        param X_MINUS_VOLTAGE: F64 default 3.3 id 14
        param X_MINUS_RESISTANCE: F64 default 13 id 15
        param X_MINUS_NUM_TURNS: F64 default 48.0 id 16
        param X_MINUS_LENGTH: F64 default 0.053 id 17
        param X_MINUS_WIDTH: F64 default 0.045 id 18
        param X_MINUS_SHAPE: MagnetorquerCoilShape default MagnetorquerCoilShape.RECTANGULAR id 34

        # --- Y+ Coil ---
        param Y_PLUS_VOLTAGE: F64 default 3.3 id 19
        param Y_PLUS_RESISTANCE: F64 default 13 id 20
        param Y_PLUS_NUM_TURNS: F64 default 48.0 id 21
        param Y_PLUS_LENGTH: F64 default 0.053 id 22
        param Y_PLUS_WIDTH: F64 default 0.045 id 23
        param Y_PLUS_SHAPE: MagnetorquerCoilShape default MagnetorquerCoilShape.RECTANGULAR id 35

        # --- Y- Coil ---
        param Y_MINUS_VOLTAGE: F64 default 3.3 id 24
        param Y_MINUS_RESISTANCE: F64 default 13 id 25
        param Y_MINUS_NUM_TURNS: F64 default 48.0 id 26
        param Y_MINUS_LENGTH: F64 default 0.053 id 27
        param Y_MINUS_WIDTH: F64 default 0.045 id 28
        param Y_MINUS_SHAPE: MagnetorquerCoilShape default MagnetorquerCoilShape.RECTANGULAR id 36

        # --- Z- Coil ---
        param Z_MINUS_VOLTAGE: F64 default 3.3 id 29
        param Z_MINUS_RESISTANCE: F64 default 150.7 id 30
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
        output port xPlusStop: Drv.StopMagnetorquer

        @ Port for triggering the X- magnetorquer
        output port xMinusStart: Drv.StartMagnetorquer

        @ Port for stopping the X- magnetorquer
        output port xMinusStop: Drv.StopMagnetorquer

        @ Port for triggering the Y+ magnetorquer
        output port yPlusStart: Drv.StartMagnetorquer

        @ Port for stopping the Y+ magnetorquer
        output port yPlusStop: Drv.StopMagnetorquer

        @ Port for triggering the Y- magnetorquer
        output port yMinusStart: Drv.StartMagnetorquer

        @ Port for stopping the Y- magnetorquer
        output port yMinusStop: Drv.StopMagnetorquer

        @ Port for triggering the Z- magnetorquer
        output port zMinusStart: Drv.StartMagnetorquer

        @ Port for stopping the Z- magnetorquer
        output port zMinusStop: Drv.StopMagnetorquer

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

        @ Rotational threshold (deg/s)
        telemetry RotationalThreshold: F64

        @ Cooldown duration
        telemetry CooldownDuration: Fw.TimeIntervalValue

        @ Torque duration
        telemetry TorqueDuration: Fw.TimeIntervalValue

        @ X+ coil voltage (V)
        telemetry XPlusVoltage: F64

        @ X+ coil resistance (Ω)
        telemetry XPlusResistance: F64

        @ X+ coil number of turns
        telemetry XPlusNumTurns: F64

        @ X+ coil length (m)
        telemetry XPlusLength: F64

        @ X+ coil width (m)
        telemetry XPlusWidth: F64

        @ X+ coil shape
        telemetry XPlusShape: MagnetorquerCoilShape

        @ X- coil voltage (V)
        telemetry XMinusVoltage: F64

        @ X- coil resistance (Ω)
        telemetry XMinusResistance: F64

        @ X- coil number of turns
        telemetry XMinusNumTurns: F64

        @ X- coil length (m)
        telemetry XMinusLength: F64

        @ X- coil width (m)
        telemetry XMinusWidth: F64

        @ X- coil shape
        telemetry XMinusShape: MagnetorquerCoilShape

        @ Y+ coil voltage (V)
        telemetry YPlusVoltage: F64

        @ Y+ coil resistance (Ω)
        telemetry YPlusResistance: F64

        @ Y+ coil number of turns
        telemetry YPlusNumTurns: F64

        @ Y+ coil length (m)
        telemetry YPlusLength: F64

        @ Y+ coil width (m)
        telemetry YPlusWidth: F64

        @ Y+ coil shape
        telemetry YPlusShape: MagnetorquerCoilShape

        @ Y- coil voltage (V)
        telemetry YMinusVoltage: F64

        @ Y- coil resistance (Ω)
        telemetry YMinusResistance: F64

        @ Y- coil number of turns
        telemetry YMinusNumTurns: F64

        @ Y- coil length (m)
        telemetry YMinusLength: F64

        @ Y- coil width (m)
        telemetry YMinusWidth: F64

        @ Y- coil shape
        telemetry YMinusShape: MagnetorquerCoilShape

        @ Z- coil voltage (V)
        telemetry ZMinusVoltage: F64

        @ Z- coil resistance (Ω)
        telemetry ZMinusResistance: F64

        @ Z- coil number of turns
        telemetry ZMinusNumTurns: F64

        @ Z- coil diameter (m)
        telemetry ZMinusDiameter: F64

        @ Z- coil shape
        telemetry ZMinusShape: MagnetorquerCoilShape

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
