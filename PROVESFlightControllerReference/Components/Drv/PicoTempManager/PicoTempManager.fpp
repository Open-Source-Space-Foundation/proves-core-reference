module Drv {
    @ Manager for the RP2350's built in temperature sensor
    passive component PicoTempManager {

        #### Ports ####
        @ Run loop
        sync input port run: Svc.Sched

        #### Commands ####
        @ Command to get the temperature in degrees Celsius
        sync command GetPicoTemperature()

        #### Telemetry ####
        @ Telemetry channel for temperature in degrees Celsius
        telemetry PicoTemperature: F64

        @ Event for reporting not ready error
        event DeviceNotReady() severity warning low format "Device not ready" throttle 5

        @ Event for reporting initialization failure
        event DeviceInitFailed(ret: I32) severity warning low format "Initialization failed with return code: {}" throttle 5

        @ Event for reporting nil device error
        event DeviceNil() severity warning low format "Device is nil" throttle 5

        @ Event for reporting nil state error
        event DeviceStateNil() severity warning low format "Device state is nil" throttle 5

        @ Event for reporting sensor fetch failure
        event SensorSampleFetchFailed(ret: I32) severity warning low format "Sensor fetch failed with return code: {}" throttle 5

        @ Event for reporting sensor channel get failure
        event SensorChannelGetFailed(ret: I32) severity warning low format "Sensor channel get failed with return code: {}" throttle 5

        @ Event for reporting temperature
        event PicoTemperature(temperature: F64) severity activity high format "Pico Temperature: {}°C"

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


    }
}
