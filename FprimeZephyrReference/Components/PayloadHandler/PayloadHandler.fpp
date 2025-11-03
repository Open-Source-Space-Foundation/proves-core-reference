module Components {
    @ Manager for Nicla Vision
    passive component PayloadHandler {

        # One async command/port is required for active components
        # This should be overridden by the developers with a useful command/port
        @ TODO
        sync command SEND_COMMAND(cmd: string)  # Command to send data over UART

        event CommandError(cmd: string) severity warning high format "Failed to send {} command over UART"

        event CommandSuccess(cmd: string) severity activity high format "Command {} sent successfully"

        event DataReceived( data: U8, path: string) severity activity high format "Stored {} bytes of payload data to {}"

        event ByteReceived( byte: U8) severity activity low format "Received byte: {}"

        event ImageHeaderReceived() severity activity low format "Received image header"

        event UartReceived() severity activity low format "Received UART data"

        output port out_port: Drv.ByteStreamSend

        sync input port in_port: Drv.ByteStreamData  

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