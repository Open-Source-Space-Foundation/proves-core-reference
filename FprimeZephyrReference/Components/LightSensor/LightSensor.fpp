module Components {
    @ Component for reading light sensor data
    passive component LightSensor {

        #### Parameters ####
        @ Parameter for setting the gain of the light senors
        param GAIN: U32 default 1

        @ Parameter for setting the integration time of the light sensors
        param INTEGRATION_TIME: U32 default 100

        @ Paremeter for setting the div4 mode of the light sensors 
        param DIV4: U32 default 0

        #### Telemetry ####
        @ Telemetry for the raw light sensor data
        telemetry RawLightData: F32

        @ Telemetry for the IR light sensor data
        telemetry IRLightData: F32

        @ Telemetry for the ALS light sensor data
        telemetry ALSLightData: F32

        #### Ports ####

        @ Port for polling the light sensor data - called by rate group
        sync input port run: Svc.Sched

        #### Events ####

        @ Event for light sensor errors
        event LightSensorError() severity warning high format "Light Sensor Error"

        @ Event for light sensor configuration 
        event LightSensorConfigured() severity activity low format "Light Sensor Configured"


        ##############################################################################
        #### Uncomment the following examples to start customizing your component ####
        ##############################################################################

        # @ Example async command
        # async command COMMAND_NAME(param_name: U32)

        # @ Example telemetry counter
        # telemetry ExampleCounter: U64

        # @ Example event
        # event ExampleStateEvent(example_state: Fw.On) severity activity high id 0 format "State set to {}"

        # @ Example port: receiving calls from the rate group
        # sync input port run: Svc.Sched

        # @ Example parameter
        # param PARAMETER_NAME: U32

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