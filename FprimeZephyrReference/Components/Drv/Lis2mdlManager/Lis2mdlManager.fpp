# Port definition
module Drv {
    port MagneticFieldGet(ref condition: Fw.Success) -> MagneticField

    @ Magnetometer sampling frequency settings for LIS2MDL sensor
    enum Lis2mdlSamplingFrequency {
        SF_10Hz @< 10 Hz sampling frequency
        SF_20Hz @< 20 Hz sampling frequency
        SF_50Hz @< 50 Hz sampling frequency
        SF_100Hz @< 100 Hz sampling frequency
    }
}

# Component definition
module Drv {
    @ LIS2MDL Manager Component for F Prime FSW framework.
    passive component Lis2mdlManager {

        ### Ports ###

        @ Port to read the current magnetic field in gauss.
        sync input port magneticFieldGet: MagneticFieldGet

        ### Parameters ###

        @ Parameter for storing the sampling frequency
        param SAMPLING_FREQUENCY: Lis2mdlSamplingFrequency default Lis2mdlSamplingFrequency.SF_100Hz id 0

        ### Telemetry ###

        @ Telemetry channel for magnetic field in gauss
        telemetry MagneticField: MagneticField

        @ Temetry channel for magnetometer sampling frequency
        telemetry SamplingFrequency: Lis2mdlSamplingFrequency

        ### Events ###

        @ Event for reporting LSM6DSO not ready error
        event DeviceNotReady() severity warning high format "LIS2MDL device not ready" throttle 5

        @ Event for reporting LIS2MDS frequency not configured
        event MagnetometerSamplingFrequencyNotConfigured() severity warning high format "LIS2MDL sampling frequency not configured" throttle 5

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
