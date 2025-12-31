module Components {
    enum CoilShape {
        RECTANGULAR, @< Rectangular coil shape
        CIRCULAR,    @< Circular coil shape
    }

    enum DetumbleMode {
        DISABLED, @< Manual override, coils forced off
        AUTO,     @< Automatic detumbling based on angular velocity
    }

    enum DetumbleState {
        COOLDOWN,                 @< Waiting for magnetometers to settle
        SENSING_ANGULAR_VELOCITY, @< Reading angular velocity
        SENSING_MAGNETIC_FIELD,   @< Reading magnetic field
        ACTUATING_BDOT,           @< Actuating the magnetorquers via B-Dot
        ACTUATING_HYSTERESIS,     @< Actuating the magnetorquers via hysteresis
    }

    enum DetumbleStrategy {
        IDLE,       @< Do not detumble
        BDOT,       @< Use B-Dot detumbling
        HYSTERESIS, @< Use hysteresis detumbling
    };

    enum HysteresisAxis {
        X_AXIS, @< X axis
        Y_AXIS, @< Y axis
        Z_AXIS, @< Z axis
    }

    port SetDetumbleMode (
        mode: DetumbleMode,     @< Desired detumble mode
    )
}

module Components {
    @ Detumble Manager Component for F Prime FSW framework.
    passive component DetumbleManager {

        ### Commands ###

        @ Command to set the operating mode
        sync command SET_MODE(mode: DetumbleMode)

        ### Parameters ###

        @ Parameter for storing the upper rotational threshold in deg/s, above which bdot detumbling is replaced by hysteresis detumbling. Given by ω_max = min(2π/∆t, π/2δT) where ∆t is the duration of actuation and δT is the time elapsed between the measurement of Ḃ.
        param BDOT_MAX_THRESHOLD: F64 default 720.0 id 40

        @ Parameter for storing the upper deadband rotational threshold in deg/s
        param DEADBAND_UPPER_THRESHOLD: F64 default 8.0 id 41

        @ Parameter for storing the lower deadband rotational threshold in deg/s, below which detumble is considered complete
        param DEADBAND_LOWER_THRESHOLD: F64 default 5.0 id 1

        @ Parameter for storing the cooldown duration
        param COOLDOWN_DURATION: Fw.TimeIntervalValue default {seconds = 0, useconds = 20000} id 3

        @ Parameter for storing the detumble torquing duration
        param TORQUE_DURATION: Fw.TimeIntervalValue default {seconds = 0, useconds = 320000} id 38

        @ Parameter for storing the gain used in the B-Dot algorithm
        param GAIN: F64 default 3.0 id 39

        @ Parameter for storing the hysteresis axis
        param HYSTERESIS_AXIS: HysteresisAxis default HysteresisAxis.X_AXIS id 42

        ### Magnetorquer Properties Parameters ###

        @ Number of turns for all coils on the X Axis
        param X_TURNS: F64 default 96.0 id 11

        @ Number of turns for all coils on the Y Axis
        param Y_TURNS: F64 default 96.0 id 21

        @ Number of turns for all coils on the Z Axis
        param Z_TURNS: F64 default 153.0 id 31

        # --- X+ Coil ---
        param X_PLUS_VOLTAGE: F64 default 3.3 id 9
        param X_PLUS_RESISTANCE: F64 default 13 id 10
        param X_PLUS_LENGTH: F64 default 0.053 id 12
        param X_PLUS_WIDTH: F64 default 0.045 id 13
        param X_PLUS_SHAPE: CoilShape default CoilShape.RECTANGULAR id 33

        # --- X- Coil ---
        param X_MINUS_VOLTAGE: F64 default 3.3 id 14
        param X_MINUS_RESISTANCE: F64 default 13 id 15
        param X_MINUS_LENGTH: F64 default 0.053 id 17
        param X_MINUS_WIDTH: F64 default 0.045 id 18
        param X_MINUS_SHAPE: CoilShape default CoilShape.RECTANGULAR id 34

        # --- Y+ Coil ---
        param Y_PLUS_VOLTAGE: F64 default 3.3 id 19
        param Y_PLUS_RESISTANCE: F64 default 13 id 20
        param Y_PLUS_LENGTH: F64 default 0.053 id 22
        param Y_PLUS_WIDTH: F64 default 0.045 id 23
        param Y_PLUS_SHAPE: CoilShape default CoilShape.RECTANGULAR id 35

        # --- Y- Coil ---
        param Y_MINUS_VOLTAGE: F64 default 3.3 id 24
        param Y_MINUS_RESISTANCE: F64 default 13 id 25
        param Y_MINUS_LENGTH: F64 default 0.053 id 27
        param Y_MINUS_WIDTH: F64 default 0.045 id 28
        param Y_MINUS_SHAPE: CoilShape default CoilShape.RECTANGULAR id 36

        # --- Z- Coil ---
        param Z_MINUS_VOLTAGE: F64 default 3.3 id 29
        param Z_MINUS_RESISTANCE: F64 default 150.7 id 30
        param Z_MINUS_DIAMETER: F64 default 0.05755 id 32
        param Z_MINUS_SHAPE: CoilShape default CoilShape.CIRCULAR id 37

        ### Ports ###

        @ Run loop
        sync input port run: Svc.Sched

        @ Port for disabling detumbling
        sync input port setMode: SetDetumbleMode

        @ Port for receiving system mode change notifications
        sync input port systemModeChanged: Components.SystemModeChanged

        @ Port for getting angular velocity magnitude readings
        output port angularVelocityMagnitudeGet: AngularVelocityMagnitudeGet

        @ Port for getting magnetic field readings in gauss
        output port magneticFieldGet: MagneticFieldGet

        @ Port to get sampling period between magnetic field reads
        output port magneticFieldSamplingPeriodGet: SamplingPeriodGet

        @ Port for querying the current system mode
        output port getSystemMode: Components.GetSystemMode

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

        @ Event for reporting failed enable attempt due to system in SAFE_MODE
        event EnableFailedSafeMode() severity warning low format "Failed to enable detumbling because system is in SAFE_MODE." throttle 5

        @ Event for recording detumbling start
        event DetumbleStarted(angular_velocity_magnitude_deg_sec: F64) severity activity low format "Detumble started. Angular velocity magnitude: {} deg/s" throttle 1

        @ Event for recording detumbling completion
        event DetumbleCompleted(angular_velocity_magnitude_deg_sec: F64) severity activity low format "Detumble complete. Angular velocity magnitude: {} deg/s" throttle 1

        @ Event for reporting magnetic field retrieval failure
        event MagneticFieldRetrievalFailed() severity warning low format "Failed to retrieve magnetic field." throttle 5

        @ Event for reporting magnetic field period retrieval failure
        event MagneticFieldSamplingPeriodRetrievalFailed() severity warning low format "Failed to retrieve magnetic field sampling frequency." throttle 5

        @ Event for reporting angular velocity retrieval failure
        event AngularVelocityRetrievalFailed() severity warning low format "Failed to retrieve angular velocity." throttle 5

        ### Telemetry ###

        @ Current operating mode
        telemetry Mode: DetumbleMode

        @ Current internal state
        telemetry State: DetumbleState

        @ Selected detumble strategy
        telemetry DetumbleStrategy: DetumbleStrategy

        @ Maximum angular velocity where BDot should be used (deg/s)
        telemetry BdotMaxThresholdParam: F64

        @ Upper deadband rotational threshold (deg/s)
        telemetry DeadbandUpperThresholdParam: F64

        @ Lower deadband rotational threshold (deg/s)
        telemetry DeadbandLowerThresholdParam: F64

        @ Gain used in B-Dot algorithm
        telemetry GainParam: F64

        @ Hysteresis axis
        telemetry HysteresisAxisParam: HysteresisAxis

        @ Cooldown duration
        telemetry CooldownDurationParam: Fw.TimeIntervalValue

        @ Torque duration parameter
        telemetry TorqueDurationParam: Fw.TimeIntervalValue

        @ Actual torque duration
        telemetry TorqueDuration: Fw.TimeIntervalValue

        @ Time between magnetic field readings
        telemetry TimeBetweenMagneticFieldReadings: Fw.TimeIntervalValue

        @ X+ coil voltage (V)
        telemetry XPlusVoltageParam: F64

        @ X+ coil resistance (Ω)
        telemetry XPlusResistanceParam: F64

        @ X+ coil number of turns
        telemetry XPlusTurnsParam: F64

        @ X+ coil length (m)
        telemetry XPlusLengthParam: F64

        @ X+ coil width (m)
        telemetry XPlusWidthParam: F64

        @ X+ coil shape
        telemetry XPlusShapeParam: CoilShape

        @ X- coil voltage (V)
        telemetry XMinusVoltageParam: F64

        @ X- coil resistance (Ω)
        telemetry XMinusResistanceParam: F64

        @ X- coil number of turns
        telemetry XMinusTurnsParam: F64

        @ X- coil length (m)
        telemetry XMinusLengthParam: F64

        @ X- coil width (m)
        telemetry XMinusWidthParam: F64

        @ X- coil shape
        telemetry XMinusShapeParam: CoilShape

        @ Y+ coil voltage (V)
        telemetry YPlusVoltageParam: F64

        @ Y+ coil resistance (Ω)
        telemetry YPlusResistanceParam: F64

        @ Y+ coil number of turns
        telemetry YPlusTurnsParam: F64

        @ Y+ coil length (m)
        telemetry YPlusLengthParam: F64

        @ Y+ coil width (m)
        telemetry YPlusWidthParam: F64

        @ Y+ coil shape
        telemetry YPlusShapeParam: CoilShape

        @ Y- coil voltage (V)
        telemetry YMinusVoltageParam: F64

        @ Y- coil resistance (Ω)
        telemetry YMinusResistanceParam: F64

        @ Y- coil number of turns
        telemetry YMinusTurnsParam: F64

        @ Y- coil length (m)
        telemetry YMinusLengthParam: F64

        @ Y- coil width (m)
        telemetry YMinusWidthParam: F64

        @ Y- coil shape
        telemetry YMinusShapeParam: CoilShape

        @ Z- coil voltage (V)
        telemetry ZMinusVoltageParam: F64

        @ Z- coil resistance (Ω)
        telemetry ZMinusResistanceParam: F64

        @ Z- coil number of turns
        telemetry ZMinusTurnsParam: F64

        @ Z- coil diameter (m)
        telemetry ZMinusDiameterParam: F64

        @ Z- coil shape
        telemetry ZMinusShapeParam: CoilShape

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
