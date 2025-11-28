module Drv {
    port temperatureGet -> F64
}

module Drv {
    @ Manager for TMP112 device
    passive component TMP112Manager {
        # Ports
        @ Port to initialize the TMP112 device
        sync input port init: Fw.SuccessCondition

        @ Port to read the temperature in degrees Celsius
        sync input port temperatureGet: temperatureGet

        # Telemetry channels

        @ Telemetry channel for temperature in degrees Celsius
        telemetry Temperature: F64

        @ Event for reporting TMP112 not ready error
        # event DeviceNotReady() severity warning high format "TMP112 device not ready"
        event DeviceNotReady() severity warning high format "TMP112 device not ready" throttle 5

        @ Event for reporting TMP112 initialization failure
        # event DeviceInitFailed(ret: I8) severity warning high format "TMP112 initialization failed with return code: {}"
        event DeviceInitFailed(ret: U8) severity warning high format "TMP112 initialization failed with return code: {}" throttle 5

        @ Event for reporting TMP112 deinitialization failure
        event DeviceDeinitFailed(ret: I8) severity warning high format "TMP112 initialization failed with return code: {}" throttle 5

        # TODO: REMOVE ME
        # event DeviceAlreadyInitialized() severity warning high format "TMP112 device already initialized"

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
