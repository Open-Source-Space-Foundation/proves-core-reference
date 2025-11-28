module Drv {
    port temperatureGet -> F64
}

module Drv {
    @ Manager for TMP112 device
    passive component TMP112Manager {
        # Ports
        # TODO(nateinaction): Load Switch output ports to initialize/deinitialize based on load switch state!!!
        # NO, it's not a 1-1 mapping because multiple sensors can be on one load switch

        @ Port to initialize the TMP112 device
        sync input port init: Fw.SuccessCondition

        @ Port to deinitialize the TMP112 device
        sync input port deinit: Fw.SuccessCondition

        @ Port to read the temperature in degrees Celsius
        sync input port temperatureGet: temperatureGet

        # TODO(nateinaction): I like the way this looks but not all tmp112 sensors will be on tca/mux or load switches
        # Example: battery and top cap temp sensors is not load switched but face temp sensors are
        @ Port to return the state of the TCA
        output port tcaHealthGet: Components.HealthGet

        @ Port to return the state of mux channel
        output port muxChannelHealthGet: Components.HealthGet

        @ Port to return the state of the load switch
        output port loadSwitchStateGet: Components.loadSwitchStateGet

        # Telemetry channels

        @ Telemetry channel for temperature in degrees Celsius
        telemetry Temperature: F64

        @ Event for reporting TMP112 not ready error
        # event DeviceNotReady() severity warning high format "TMP112 device not ready"
        event DeviceNotReady() severity warning high format "TMP112 device not ready" throttle 5

        @ Event for reporting TMP112 initialization failure
        event DeviceInitFailed(ret: I32) severity warning high format "TMP112 initialization failed with return code: {}" throttle 5

        @ Event for reporting TMP112 sensor fetch failure
        event SensorFetchFailed(ret: I32) severity warning high format "TMP112 sensor fetch failed with return code: {}" throttle 5

        @ Event for reporting TMP112 sensor channel get failure
        event SensorChannelGetFailed(ret: I32) severity warning high format "TMP112 sensor channel get failed with return code: {}" throttle 5

        @ Event for reporting TMP112 deinitialization failure
        event DeviceDeinitFailed() severity warning high format "TMP112 deinitialization failed" throttle 5

        @ Event for reporting TMP112 sensor fetch failure
        event SensorFetchFailed(ret: I32) severity warning high format "TMP112 sensor fetch failed with return code: {}" throttle 5

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
