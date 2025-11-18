module Components {
    @ Barebones UART communication layer for payload (Nicla Vision camera)
    @ Handles UART forwarding and ACK handshake, protocol processing done by specific payload handler
    passive component PayloadCom {

        event CommandForwardError(cmd: string) severity warning high format "Failed to send {} command over UART"

        event CommandForwardSuccess(cmd: string) severity activity high format "Command {} sent successfully"

        event UartReceived() severity activity low format "Received UART data"

        event AckSent() severity activity low format "ACK sent to payload"

        @ Receives the desired command to forward through the payload UART
        sync input port commandIn: Drv.ByteStreamData

        @ Receives data from the UART, forwards to handler and sends ACK
        sync input port uartDataIn: Drv.ByteStreamData

        @ Sends data to the UART (forwards commands and acknowledgement signals)
        output port uartForward: Drv.ByteStreamSend

        @ Return RX buffers to UART driver (driver will deallocate to BufferManager)
        output port bufferReturn: Fw.BufferSend

        @ Sends data that is received by the uartDataIn port to the desired payload handler component
        output port uartDataOut: Drv.ByteStreamData

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
