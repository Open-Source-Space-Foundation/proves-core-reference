# Port definition
module Drv {
    port DistanceGet -> F64
}

# Component definition
module Drv {
    @ VL6180 Distance Sensor Driver Component for F Prime FSW framework.
    passive component DistanceSensorManager {
        # Ports
        @ Port to read the current distance in millimeters.
        sync input port distanceGet: DistanceGet

        @ Port to trigger periodic distance reading (called by rate group).
        sync input port run: Svc.Sched

        # Commands
        @ Command to manually trigger a distance reading
        sync command READ_DISTANCE()

        # Telemetry channels
        @ Telemetry channel for current distance in millimeters.
        telemetry Distance: F64

        @ Event for reporting VL6180 not ready error
        event DeviceNotReady() severity warning high format "VL6180 device not ready" throttle 5

        @ Event for reporting VL6180 initialization error
        event InitializationFailed() severity warning high format "VL6180 initialization failed"

        @ Event for reporting I2C communication error
        event I2cError() severity warning high format "VL6180 I2C communication error" throttle 5

        @ Event for reporting successful distance reading
        event DistanceReading(
            distance: F64 @< Distance reading in millimeters
        ) \
            severity activity low \
            format "VL6180 distance: {} mm"

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for receiving commands
        command recv port cmdIn

        @ Port for sending command registrations
        command reg port cmdRegOut

        @ Port for sending command responses
        command resp port cmdResponseOut
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
