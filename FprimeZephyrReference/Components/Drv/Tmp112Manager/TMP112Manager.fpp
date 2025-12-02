module Drv {
    port temperatureGet(ref condition: Fw.Success) -> F64
}

module Drv {
    @ Manager for TMP112 device
    passive component Tmp112Manager {
        # Ports
        @ Port to read the temperature in degrees Celsius
        sync input port temperatureGet: temperatureGet

        @ Port to initialize and deinitialize the TMP112 device on load switch state change
        sync input port loadSwitchStateChanged: Components.loadSwitchStateChanged

        # Commands
        # @ Command to get the temperature from the TMP112 device
        # sync command GET_TEMPERATURE(
        #     $port: FwIndexType @< Temperature Sensor to read
        # )

        # Events
        @ Event for reporting TMP112 not ready error
        event DeviceNotReady() severity warning high format "TMP112 device not ready" throttle 5

        @ Event for reporting TMP112 initialization failure
        event DeviceInitFailed(ret: I32) severity warning high format "TMP112 initialization failed with return code: {}" throttle 5

        @ Event for reporting TMP112 nil device error
        event DeviceNil() severity warning high format "TMP112 device is nil" throttle 5

        @ Event for reporting TMP112 nil state error
        event DeviceStateNil() severity warning high format "TMP112 device state is nil" throttle 5

        @ Event for reporting TCA unhealthy state
        event TcaUnhealthy() severity warning high format "TMP112 TCA device is unhealthy" throttle 5

        @ Event for reporting MUX unhealthy state
        event MuxUnhealthy() severity warning high format "TMP112 MUX device is unhealthy" throttle 5

        @ Event for reporting Load Switch not ready state
        event LoadSwitchNotReady() severity warning high format "TMP112 Load Switch is not ready" throttle 5

        @ Event for reporting TMP112 sensor fetch failure
        event SensorSampleFetchFailed(ret: I32) severity warning high format "TMP112 sensor fetch failed with return code: {}" throttle 5

        @ Event for reporting TMP112 sensor channel get failure
        event SensorChannelGetFailed(ret: I32) severity warning high format "TMP112 sensor channel get failed with return code: {}" throttle 5

        @ Event to report TMP112 temperature readout
        event TemperatureReadout($port: FwIndexType, tempC: F64) severity activity high format "TMP112 {} Temperature Readout: {} °C"

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

    }
}
