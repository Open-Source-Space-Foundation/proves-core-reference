module Components {
    enum FpCoilShape {
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

    enum FpDetumbleStrategy {
        IDLE,       @< Do not detumble
        BDOT,       @< Use B-Dot detumbling
        HYSTERESIS, @<Use hysteresis detumbling
    };
}

module Components {
    @ Detumble Manager Component for F Prime FSW framework.
    passive component DetumbleManager {

        ### Parameters ###

        @ Parameter for storing the operating mode (default DISABLED)
        param OPERATING_MODE: DetumbleMode default DetumbleMode.DISABLED id 0

        @ Parameter for storing the upper rotational threshold in deg/s, above which bdot detumbling is replaced by hysteresis detumbling
        param BDOT_MAX_THRESHOLD: F64 default 30.0 id 40

        @ Parameter for storing the upper deadband rotational threshold in deg/s
        param DEADBAND_UPPER_THRESHOLD: F64 default 18.0 id 41

        @ Parameter for storing the lower deadband rotational threshold in deg/s, below which detumble is considered complete
        param DEADBAND_LOWER_THRESHOLD: F64 default 12.0 id 1

        @ Parameter for storing the cooldown duration
        param COOLDOWN_DURATION: Fw.TimeIntervalValue default {seconds = 0, useconds = 20000} id 3

        @ Parameter for storing the detumble torquing duration
        param TORQUE_DURATION: Fw.TimeIntervalValue default {seconds = 0, useconds = 20000} id 38

        @ Gain used for B-Dot algorithm
        param GAIN: F64 default 2.0 id 39

        ### Magnetorquer Properties Parameters ###

        # --- X+ Coil ---
        param X_PLUS_VOLTAGE: F64 default 3.3 id 9
        param X_PLUS_RESISTANCE: F64 default 13 id 10
        param X_PLUS_TURNS: F64 default 48.0 id 11
        param X_PLUS_LENGTH: F64 default 0.053 id 12
        param X_PLUS_WIDTH: F64 default 0.045 id 13
        param X_PLUS_SHAPE: FpCoilShape default FpCoilShape.RECTANGULAR id 33

        # --- X- Coil ---
        param X_MINUS_VOLTAGE: F64 default 3.3 id 14
        param X_MINUS_RESISTANCE: F64 default 13 id 15
        param X_MINUS_TURNS: F64 default 48.0 id 16
        param X_MINUS_LENGTH: F64 default 0.053 id 17
        param X_MINUS_WIDTH: F64 default 0.045 id 18
        param X_MINUS_SHAPE: FpCoilShape default FpCoilShape.RECTANGULAR id 34

        # --- Y+ Coil ---
        param Y_PLUS_VOLTAGE: F64 default 3.3 id 19
        param Y_PLUS_RESISTANCE: F64 default 13 id 20
        param Y_PLUS_TURNS: F64 default 48.0 id 21
        param Y_PLUS_LENGTH: F64 default 0.053 id 22
        param Y_PLUS_WIDTH: F64 default 0.045 id 23
        param Y_PLUS_SHAPE: FpCoilShape default FpCoilShape.RECTANGULAR id 35

        # --- Y- Coil ---
        param Y_MINUS_VOLTAGE: F64 default 3.3 id 24
        param Y_MINUS_RESISTANCE: F64 default 13 id 25
        param Y_MINUS_TURNS: F64 default 48.0 id 26
        param Y_MINUS_LENGTH: F64 default 0.053 id 27
        param Y_MINUS_WIDTH: F64 default 0.045 id 28
        param Y_MINUS_SHAPE: FpCoilShape default FpCoilShape.RECTANGULAR id 36

        # --- Z- Coil ---
        param Z_MINUS_VOLTAGE: F64 default 3.3 id 29
        param Z_MINUS_RESISTANCE: F64 default 150.7 id 30
        param Z_MINUS_TURNS: F64 default 153.0 id 31
        param Z_MINUS_DIAMETER: F64 default 0.05755 id 32
        param Z_MINUS_SHAPE: FpCoilShape default FpCoilShape.CIRCULAR id 37

        ### Ports ###

        @ Run loop
        sync input port run: Svc.Sched

        @ Port for getting angular velocity readings in rad/s
        output port angularVelocityGet: AngularVelocityGet

        @ Port for getting magnetic field readings in gauss
        output port magneticFieldGet: MagneticFieldGet

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

        @ Event for reporting magnetic field retrieval failure
        event MagneticFieldRetrievalFailed() severity warning low format "Failed to retrieve magnetic field." throttle 5

        @ Event for reporting magnetic field too small for dipole moment calculation
        event MagneticFieldTooSmallForDipoleMoment() severity warning low format "Magnetic field magnitude too small to compute dipole moment." throttle 5

        @ Event for reporting invalid magnetic field readings for dipole moment calculation
        event InvalidMagneticFieldReadingForDipoleMoment() severity warning low format "Out of order readings or readings taken too quickly, failed to compute dipole moment" throttle 5

        @ Event for reporting unknown dipole moment computation error
        event UnknownDipoleMomentComputationError() severity warning low format "Unknown error occurred during dipole moment computation." throttle 5

        @ Event for reporting invalid detumble strategy selection
        event InvalidDetumbleStrategy(strategy: FpDetumbleStrategy) severity warning low format "Invalid detumble strategy selected: %d" throttle 5

        @ Event for reporting angular velocity retrieval failure
        event AngularVelocityRetrievalFailed() severity warning low format "Failed to retrieve angular velocity." throttle 5

        ### Telemetry ###

        @ Current operating mode
        telemetry Mode: DetumbleMode

        @ Current internal state
        telemetry State: DetumbleState

        @ Selected detumble strategy
        telemetry DetumbleStrategy: FpDetumbleStrategy

        @ Maximum angular velocity where BDot should be used (deg/s)
        telemetry BdotMaxThreshold: F64

        @ Upper deadband rotational threshold (deg/s)
        telemetry DeadbandUpperThreshold: F64

        @ Lower deadband rotational threshold (deg/s)
        telemetry DeadbandLowerThreshold: F64

        @ Gain used in B-Dot algorithm
        telemetry Gain: F64

        @ Cooldown duration
        telemetry CooldownDuration: Fw.TimeIntervalValue

        @ Torque duration
        telemetry TorqueDuration: Fw.TimeIntervalValue

        @ X+ coil voltage (V)
        telemetry XPlusVoltage: F64

        @ X+ coil resistance (Ω)
        telemetry XPlusResistance: F64

        @ X+ coil number of turns
        telemetry XPlusTurns: F64

        @ X+ coil length (m)
        telemetry XPlusLength: F64

        @ X+ coil width (m)
        telemetry XPlusWidth: F64

        @ X+ coil shape
        telemetry XPlusShape: FpCoilShape

        @ X- coil voltage (V)
        telemetry XMinusVoltage: F64

        @ X- coil resistance (Ω)
        telemetry XMinusResistance: F64

        @ X- coil number of turns
        telemetry XMinusTurns: F64

        @ X- coil length (m)
        telemetry XMinusLength: F64

        @ X- coil width (m)
        telemetry XMinusWidth: F64

        @ X- coil shape
        telemetry XMinusShape: FpCoilShape

        @ Y+ coil voltage (V)
        telemetry YPlusVoltage: F64

        @ Y+ coil resistance (Ω)
        telemetry YPlusResistance: F64

        @ Y+ coil number of turns
        telemetry YPlusTurns: F64

        @ Y+ coil length (m)
        telemetry YPlusLength: F64

        @ Y+ coil width (m)
        telemetry YPlusWidth: F64

        @ Y+ coil shape
        telemetry YPlusShape: FpCoilShape

        @ Y- coil voltage (V)
        telemetry YMinusVoltage: F64

        @ Y- coil resistance (Ω)
        telemetry YMinusResistance: F64

        @ Y- coil number of turns
        telemetry YMinusTurns: F64

        @ Y- coil length (m)
        telemetry YMinusLength: F64

        @ Y- coil width (m)
        telemetry YMinusWidth: F64

        @ Y- coil shape
        telemetry YMinusShape: FpCoilShape

        @ Z- coil voltage (V)
        telemetry ZMinusVoltage: F64

        @ Z- coil resistance (Ω)
        telemetry ZMinusResistance: F64

        @ Z- coil number of turns
        telemetry ZMinusTurns: F64

        @ Z- coil diameter (m)
        telemetry ZMinusDiameter: F64

        @ Z- coil shape
        telemetry ZMinusShape: FpCoilShape

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
