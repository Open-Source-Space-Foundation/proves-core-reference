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
        VEML6031_IT_800 @< 800 ms integration time
    }

    @ Effective photodiode size (DIV4) settings for VEML6031 sensor
    enum DIV4 : U8 {
        VEML6031_SIZE_4_4
        VEML6031_SIZE_1_4
    }

    @ Ambient Light Sensor (ALS) persistence protect number settings for VEML6031 sensor
    enum ALS_PERS : U8 {
        VEML60XX_PERS_1 @< Interrupt triggered every ALS reading
        VEML60XX_PERS_2 @< Interrupt triggered after 2 consecutive ALS readings out of threshold
        VEML60XX_PERS_4 @< Interrupt triggered after 4 consecutive ALS readings out of threshold
        VEML60XX_PERS_8 @< Interrupt triggered after 8 consecutive ALS readings out of threshold
    }
}

module Drv {
    passive component Veml6031Manager {

        #### Parameters ####
        @ Parameter for setting the gain of the light senors
        param GAIN: GAIN default GAIN.VEML6031_GAIN_1

        @ Parameter for setting the integration time (IT) of the light sensors
        param INTEGRATION_TIME: IT default IT.VEML6031_IT_100

        @ Parameter for setting the effective photodiode size (DIV4) mode of the light sensors
        param EFFECTIVE_PHOTODIODE_SIZE: DIV4 default DIV4.VEML6031_SIZE_4_4

        @ Parameter for setting the Ambient Light Sensor (ALS) persistence protect number setting (PERS).
        param ALS_PERSISTENCE_PROTECT_NUMBER: ALS_PERS default ALS_PERS.VEML60XX_PERS_1

        #### Telemetry ####
        @ Telemetry for the illuminance in the visible spectrum, in lux
        telemetry VisibleLight: F32

        @ Telemetry for the illuminance in the infra-red spectrum, in lux
        telemetry InfraRedLight: F32

        @ Telemetry for the ambient illuminance in visible spectrum, in lux
        telemetry AmbientLight: F32

        #### Ports ####

        @ Port to read the illuminance in visible spectrum, in lux
        @ This channel represents the raw measurement counts provided by the sensor ALS register.
        @ It is useful for estimating good values for integration time, effective photodiode size
        @ and gain attributes in fetch and get mode.
        sync input port visibleLightGet: lightGet

        @ Port to read the illuminance in infra-red spectrum, in lux
        sync input port infraRedLightGet: lightGet

        @ Port to read the ambient illuminance in visible spectrum, in lux
        sync input port ambientLightGet: lightGet

        @ Port to initialize and deinitialize the VEML6031 device on load switch state change
        sync input port loadSwitchStateChanged: Components.loadSwitchStateChanged

        #### Events ####

        @ Event for reporting VEML6031 not ready error
        event DeviceNotReady() severity warning high format "VEML6031 device not ready" throttle 5

        @ Event for reporting VEML6031 initialization failure
        event DeviceInitFailed(ret: I32) severity warning high format "VEML6031 initialization failed with return code: {}" throttle 5

        @ Event for reporting VEML6031 nil device error
        event DeviceNil() severity warning high format "VEML6031 device is nil" throttle 5

        @ Event for reporting VEML6031 nil state error
        event DeviceStateNil() severity warning high format "VEML6031 device state is nil" throttle 5

        @ Event for reporting TCA unhealthy state
        event TcaUnhealthy() severity warning high format "VEML6031 TCA device is unhealthy" throttle 5

        @ Event for reporting MUX unhealthy state
        event MuxUnhealthy() severity warning high format "VEML6031 MUX device is unhealthy" throttle 5

        @ Event for reporting Load Switch not ready state
        event LoadSwitchNotReady() severity warning high format "VEML6031 Load Switch is not ready" throttle 5

        @ Event for reporting VEML6031 sensor fetch failure
        event SensorSampleFetchFailed(ret: I32) severity warning high format "VEML6031 sensor fetch failed with return code: {}" throttle 5

        @ Event for reporting VEML6031 sensor channel get failure
        event SensorChannelGetFailed(ret: I32) severity warning high format "VEML6031 sensor channel get failed with return code: {}" throttle 5

        @ Event for reporting invalid gain parameter
        event InvalidGainParam(gain: U8) severity warning high format "VEML6031 invalid gain parameter: {}" throttle 5

        @ Event for reporting invalid integration time parameter
        event InvalidIntegrationTimeParam(it: U8) severity warning high format "VEML6031 invalid integration time parameter: {}" throttle 5

        @ Event for reporting invalid effective photodiode size parameter
        event InvalidDiv4Param(div4: U8) severity warning high format "VEML6031 invalid effective photodiode size parameter: {}" throttle 5

        @ Event for reporting sensor attribute set failure
        event SensorAttrSetFailed(attr: U16, val: U8, ret: I32) severity warning high format "VEML6031 sensor attribute {}={} set failed with return code: {}" throttle 5

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
