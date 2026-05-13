module Components {
    port AccelerationGet(ref condition: Fw.Success) -> Drv.Acceleration
    port AngularVelocityGet(ref condition: Fw.Success)-> Drv.AngularVelocity
    port AngularVelocityMagnitudeGet(ref condition: Fw.Success, unit: AngularUnit) -> F64
    port MagneticFieldGet(ref condition: Fw.Success) -> Drv.MagneticField
    port SamplingPeriodGet(ref condition: Fw.Success) -> Fw.TimeIntervalValue

    @ Magnetometer sampling frequency settings for LIS2MDL sensor
    enum Lis2mdlSamplingFrequency {
        SF_10Hz @< 10 Hz sampling frequency
        SF_20Hz @< 20 Hz sampling frequency
        SF_50Hz @< 50 Hz sampling frequency
        SF_100Hz @< 100 Hz sampling frequency
    }

    @ Accelerometer and Gyroscope sampling frequency settings for LSM6DSO sensor
    enum Lsm6dsoSamplingFrequency {
        SF_12_5Hz @< 12.5 Hz sampling frequency
        SF_26Hz @< 26 Hz sampling frequency
        SF_52Hz @< 52 Hz sampling frequency
        SF_104Hz @< 104 Hz sampling frequency
        SF_208Hz @< 208 Hz sampling frequency
        SF_416Hz @< 416 Hz sampling frequency
        SF_833Hz @< 833 Hz sampling frequency
        SF_1_66kHz @< 1.66 kHz sampling frequency
        SF_3_33kHz @< 3.33 kHz sampling frequency
        SF_6_66kHz @< 6.66 kHz sampling frequency
    }

    @ Axis orientation settings
    enum AxisOrientation {
        STANDARD @< Standard orientation
        ROTATED_90_DEG_CW @< Rotated 90 degrees clockwise
        ROTATED_90_DEG_CCW @< Rotated 90 degrees counter-clockwise
        ROTATED_180_DEG @< Rotated 180 degrees
    }

    @ Units for angular velocity
    enum AngularUnit {
        RAD_PER_SEC @< Radians per second
        DEG_PER_SEC @< Degrees per second
    }
}

module Components {
    @ IMU Manager Component for F Prime FSW framework.
    passive component ImuManager {

        ### Ports ###

        @ Port to trigger periodic data fetching and telemetry updating
        sync input port run: Svc.Sched

        @ Port to read the current acceleration in m/s^2.
        sync input port accelerationGet: AccelerationGet

        @ Port to read the current angular velocity in rad/s.
        sync input port angularVelocityGet: AngularVelocityGet

        @ Port to read the current angular velocity magnitude.
        sync input port angularVelocityMagnitudeGet: AngularVelocityMagnitudeGet

        @ Port to read the current magnetic field in gauss.
        sync input port magneticFieldGet: MagneticFieldGet

        @ Port to get the time between magnetic field reads
        sync input port magneticFieldSamplingPeriodGet: SamplingPeriodGet

        ### Parameters ###

        @ Parameter for storing the accelerometer sampling frequency
        param ACCELEROMETER_SAMPLING_FREQUENCY: Lsm6dsoSamplingFrequency default Lsm6dsoSamplingFrequency.SF_12_5Hz id 0

        @ Parameter for storing the gyroscope sampling frequency
        param GYROSCOPE_SAMPLING_FREQUENCY: Lsm6dsoSamplingFrequency default Lsm6dsoSamplingFrequency.SF_104Hz id 1

        @ Parameter for storing the magnetometer sampling frequency
        param MAGNETOMETER_SAMPLING_FREQUENCY: Lis2mdlSamplingFrequency default Lis2mdlSamplingFrequency.SF_100Hz id 2

        @ Parameter for storing the axis orientation
        param AXIS_ORIENTATION: AxisOrientation default AxisOrientation.STANDARD id 3

        ### Telemetry channels ###

        @ Telemetry channel for axis orientation
        telemetry AxisOrientation: AxisOrientation

        @ Telemetry channel for current acceleration in m/s^2.
        telemetry Acceleration: Drv.Acceleration

        @ Telemetry channel for current angular velocity in rad/s.
        telemetry AngularVelocity: Drv.AngularVelocity

        @ Telemetry channel for current magnetic field in gauss.
        telemetry MagneticField: Drv.MagneticField

        @ Accelerometer sampling frequency telemetry channel
        telemetry AccelerometerSamplingFrequency: Lsm6dsoSamplingFrequency

        @ Gyroscope sampling frequency telemetry channel
        telemetry GyroscopeSamplingFrequency: Lsm6dsoSamplingFrequency

        @ Temetry channel for magnetometer sampling frequency
        telemetry MagnetometerSamplingFrequency: Lis2mdlSamplingFrequency

        ### Events ###

        @ Event for reporting LIS2MDL not ready error
        event Lis2mdlDeviceNotReady() severity warning high format "LIS2MDL device not ready" throttle 5

        @ Event for reporting LSM6DSO not ready error
        event Lsm6dsoDeviceNotReady() severity warning high format "LSM6DSO device not ready" throttle 5

        @ Event for reporting LSM6DSO accelerometer sampling frequency not configured error
        event AccelerometerSamplingFrequencyNotConfigured() severity warning high format "LSM6DSO accelerometer sampling frequency not configured" throttle 5

        @ Event for reporting LSM6DSO gyroscope sampling frequency not configured error
        event GyroscopeSamplingFrequencyNotConfigured() severity warning high format "LSM6DSO gyroscope sampling frequency not configured" throttle 5

        @ Event for reporting LIS2MDS magnetometer frequency not configured
        event MagnetometerSamplingFrequencyNotConfigured() severity warning high format "LIS2MDL magnetomiter sampling frequency not configured" throttle 5

        @ Event for reporting failure to retrieve LIS2MDS magnetometer sampling frequency
        event MagnetometerSamplingFrequencyGetFailed() severity warning low format "Failed to retrieve LIS2MDL magnetometer sampling frequency" throttle 5

        @ Event to report LIS2MDL magnetometer sampling frequency of 0 Hz
        event MagnetometerSamplingFrequencyZeroHz() severity warning low format "LIS2MDL magnetometer sampling frequency is set to 0 Hz" throttle 5

        @ Event to report acceleration data
        event AccelerationData(x: F64, y: F64, z: F64) severity activity low format "Acceleration: x={} m/s^2, y={} m/s^2, z={} m/s^2"

        @ Event to report angular velocity data
        event AngularVelocityData(x: F64, y: F64, z: F64) severity activity low format "Angular Velocity: x={} rad/s, y={} rad/s, z={} rad/s"

        @ Event to report magnetic field data
        event MagneticFieldData(x: F64, y: F64, z: F64) severity activity low format "Magnetic Field: x={} gauss, y={} gauss, z={} gauss"

        ### Commands ###

        @ Command to get the current acceleration
        sync command GET_ACCELERATION()

        @ Command to get the current angular velocity
        sync command GET_ANGULAR_VELOCITY()

        @ Command to get the current magnetic field
        sync command GET_MAGNETIC_FIELD()

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
