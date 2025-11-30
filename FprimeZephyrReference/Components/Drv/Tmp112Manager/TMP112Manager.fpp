module Drv {
    port temperatureGet -> F64
    port initialized -> bool
}

module Drv {
    @ Manager for TMP112 device
    passive component TMP112Manager {
        # Ports
        @ Port to initialize the TMP112 device
        sync input port init: Fw.SuccessCondition

        @ Port to read the temperature in degrees Celsius
        sync input port temperatureGet: temperatureGet

        @ Output port to check device TCA health
        output port getTCAHealth: Components.HealthGet

        @ Output port to check device MUX health
        output port getMUXHealth: Components.HealthGet

        @ Output port to get load switch state
        output port getLoadSwitchState: Components.loadSwitchStateGet

        # Telemetry channels

        @ Telemetry channel for temperature in degrees Celsius
        telemetry Temperature: F64

        @ Event for reporting TMP112 not ready error
        # event DeviceNotReady() severity warning high format "TMP112 device not ready"
        event DeviceNotReady() severity warning high format "TMP112 device not ready" throttle 5

        @ Event for reporting TMP112 initialization failure
        event DeviceInitFailed(ret: I32) severity warning high format "TMP112 initialization failed with return code: {}" throttle 5

        @ Event for reporting TMP112 nil device error
        event DeviceNil() severity warning high format "TMP112 device is nil" throttle 5

        @ Event for reporting TMP112 nil state error
        event DeviceStateNil() severity warning high format "TMP112 device state is nil" throttle 5

        @ Event for reporting TMP112 sensor fetch failure
        event SensorSampleFetchFailed(ret: I32) severity warning high format "TMP112 sensor fetch failed with return code: {}" throttle 5

        @ Event for reporting TMP112 sensor channel get failure
        event SensorChannelGetFailed(ret: I32) severity warning high format "TMP112 sensor channel get failed with return code: {}" throttle 5

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

    }
}
