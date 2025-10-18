module LightSensor {
    @ Component to deliver data from the VEML 6031 light sensor using the Zephyr driver
    passive component LightSensorManager {

        @ Command to turn the light sensor off and on
        sync command RESET

        @ Telemetry channel to report light sensor reading
        telemetry Reading: LightSensorData

        @ Telemetry channel to report whether reading is valid
        telemetry Valid: bool

        @ Event to report if light is detected
        event LightDetected(
            lux : I32 @<Measured Light Level in Lux>
        ) severity activity high format "Light detected: {} lux";

        @ Event to report if light sensor is turned on
        event LightSensorTurnedOn() severity activity high format "Light sensor turned on";

        @ Event to report if light sensor is turned off
        event LightSensorTurnedOff() severity activity high format "Light sensor turned off";

        @ Event to report if light sensor reset
        event LightSensorReset() severity activity high format "Light sensor reset";

        @ Event to report if light sensor entered to error state
        event LightSensorError(
            error_msg: string size 40 @<Error message to output>
        ) severity warning high format "Light sensor error: {}";

        @ Sensitivity to light
        param GAIN: U32

        @ How long the sensor reads before taking a measurement
        param INTEGRATION_TIME: U32

        @ Threshold to determine if light is detected
        param DET_THRESHOLD: U32

        @ Reading light sensor every x rate group ticks
        param READ_INTERVAL: U32 default 1

        @ Port for receiving call from load switch
        async input port loadSwitch: Drv.GpioRead

        @ Port receiving calls from the rate group
        sync input port run: Svc.Sched

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