module Drv {
    port lightGet(ref condition: Fw.Success) -> F32

    @ Gain settings for VEML6031 sensor
    enum GAIN : U8 {
        VEML6031_GAIN_1 @< Gain of 1
        VEML6031_GAIN_2 @< Gain of 2
        VEML6031_GAIN_0_66 @< Gain of 0.66
        VEML6031_GAIN_0_5 @< Gain of 0.5
    }

    @ Integration time settings for VEML6031 sensor
    enum IT : U8 {
        VEML6031_IT_3_125 @< 3.25 ms integration time
        VEML6031_IT_6_25 @< 6.25 ms integration time
        VEML6031_IT_12_5 @< 12.5 ms integration time
        VEML6031_IT_25 @< 25 ms integration time
        VEML6031_IT_50 @< 50 ms integration time
        VEML6031_IT_100 @< 100 ms integration time
        VEML6031_IT_200 @< 200 ms integration time
        VEML6031_IT_400 @< 400 ms integration time
    }

    @ Effective photodiode size (DIV4) settings for VEML6031 sensor
    enum DIV4 : U8 {
        VEML6031_SIZE_4_4
        VEML6031_SIZE_1_4
    }
}

module Drv {
    @ Manager for VEML6031 device
    passive component Veml6031Manager {

        #### Parameters ####
        @ Parameter for setting the gain of the light senors
        param GAIN: GAIN default GAIN.VEML6031_GAIN_0_5

        @ Parameter for setting the integration time (IT) of the light sensors
        param INTEGRATION_TIME: IT default IT.VEML6031_IT_25

        @ Parameter for setting the effective photodiode size (DIV4) mode of the light sensors
        param EFFECTIVE_PHOTODIODE_SIZE: DIV4 default DIV4.VEML6031_SIZE_1_4

        #### Commands ####
        @ Command to get the visible light measurement in lux
        sync command GetVisibleLight()

        #### Telemetry ####
        @ Telemetry for the illuminance in the visible spectrum, in lux
        telemetry VisibleLight: F32

        #### Ports ####

        @ Port to read the illuminance in visible spectrum, in lux
        @ This channel represents the raw measurement counts provided by the sensor ALS register.
        @ It is useful for estimating good values for integration time, effective photodiode size
        @ and gain attributes in fetch and get mode.
        sync input port visibleLightGet: lightGet

        @ Port to initialize and deinitialize the device on load switch state change
        sync input port loadSwitchStateChanged: Components.loadSwitchStateChanged

        #### Events ####

        @ Event for reporting not ready error
        event DeviceNotReady() severity warning low format "Device not ready" throttle 5

        @ Event for reporting initialization failure
        event DeviceInitFailed(ret: I32) severity warning low format "Initialization failed with return code: {}" throttle 5

        @ Event for reporting nil device error
        event DeviceNil() severity warning low format "Device is nil" throttle 5

        @ Event for reporting nil state error
        event DeviceStateNil() severity warning low format "Device state is nil" throttle 5

        @ Event for reporting TCA unhealthy state
        event TcaUnhealthy() severity warning low format "TCA device is unhealthy" throttle 5

        @ Event for reporting MUX unhealthy state
        event MuxUnhealthy() severity warning low format "MUX device is unhealthy" throttle 5

        @ Event for reporting sensor fetch failure
        event SensorSampleFetchFailed(ret: I32) severity warning low format "Sensor fetch failed with return code: {}" throttle 5

        @ Event for reporting sensor channel get failure
        event SensorChannelGetFailed(ret: I32) severity warning low format "Sensor channel get failed with return code: {}" throttle 5

        @ Event for reporting invalid gain parameter
        event InvalidGainParam(gain: U8) severity warning low format "Invalid gain parameter: {}" throttle 5

        @ Event for reporting invalid integration time parameter
        event InvalidIntegrationTimeParam(it: U8) severity warning low format "Invalid integration time parameter: {}" throttle 5

        @ Event for reporting invalid effective photodiode size parameter
        event InvalidDiv4Param(div4: U8) severity warning low format "Invalid effective photodiode size parameter: {}" throttle 5

        @ Event for reporting sensor attribute set failure
        event SensorAttrSetFailed(attr: string, val: U8, ret: I32) severity warning low format "Sensor attribute {}={} set failed with return code: {}" throttle 5

        @ Event for reporting visible light lux
        event VisibleLight(lux: F32) severity activity high format "Visible light: {} lux"

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
