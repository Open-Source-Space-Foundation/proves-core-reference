module Components {
    port AccelerationGet(ref condition: Fw.Success) -> Drv.Acceleration
    port AngularVelocityGet(ref condition: Fw.Success)-> Drv.AngularVelocity
    port MagneticFieldGet(ref condition: Fw.Success) -> Drv.MagneticField

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

        @ Port to read the current magnetic field in gauss.
        sync input port magneticFieldGet: MagneticFieldGet

        @ Port for sending accelerationGet calls to the LSM6DSO Driver
        output port acceleration: AccelerationGet

        @ Port for sending angularVelocityGet calls to the LSM6DSO Driver
        output port angularVelocity: AngularVelocityGet

        @ Port for sending magneticFieldGet calls to the LIS2MDL Manager
        output port magneticField: MagneticFieldGet

        ### Parameters ###

        @ Parameter for storing the accelerometer sampling frequency
        param ACCELEROMETER_SAMPLING_FREQUENCY: Lsm6dsoSamplingFrequency default Lsm6dsoSamplingFrequency.SF_12_5Hz id 0

        @ Parameter for storing the gyroscope sampling frequency
        param GYROSCOPE_SAMPLING_FREQUENCY: Lsm6dsoSamplingFrequency default Lsm6dsoSamplingFrequency.SF_12_5Hz id 1

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
