module Drv {
    port AmbientTemperatureGet -> F64
}

module Drv {
    @ Manager for TMP112 device
    passive component TMP112Manager {
        # Ports
        @ Port to read the ambient temperature in degrees Celsius
        sync input port ambientTemperatureGet: AmbientTemperatureGet

        # Telemetry channels

        @ Telemetry channel for ambient temperature in degrees Celsius
        telemetry AmbientTemperature: F64

        @ Event for reporting TMP112 not ready error
        event DeviceNotReady() severity warning high format "TMP112 device not ready" throttle 5

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
