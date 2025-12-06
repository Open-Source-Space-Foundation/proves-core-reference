module Drv {
    port temperatureGet(ref condition: Fw.Success) -> F64
}

module Drv {
    @ Manager for TMP112 device
    passive component Tmp112Manager {
        # Ports
        @ Port to read the temperature in degrees Celsius
        sync input port temperatureGet: temperatureGet

        @ Port to initialize and deinitialize the device on load switch state change
        sync input port loadSwitchStateChanged: Components.loadSwitchStateChanged

        # Commands
        @ Command to get the temperature in degrees Celsius
        sync command GetTemperature()

        # Telemetry channels

        @ Telemetry channel for temperature in degrees Celsius
        telemetry Temperature: F64

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

        @ Event for reporting Load Switch not ready state
        event LoadSwitchNotReady() severity warning low format "Load Switch is not ready" throttle 5

        @ Event for reporting sensor fetch failure
        event SensorSampleFetchFailed(ret: I32) severity warning low format "Sensor fetch failed with return code: {}" throttle 5

        @ Event for reporting sensor channel get failure
        event SensorChannelGetFailed(ret: I32) severity warning low format "Sensor channel get failed with return code: {}" throttle 5

        @ Event for reporting temperature
        event Temperature(temperature: F64) severity activity high format "Temperature: {}°C"

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
