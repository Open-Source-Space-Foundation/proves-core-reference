module Components {
    @ Manager for Nicla Vision
    passive component PayloadCom {

        event CommandForwardError(cmd: string) severity warning high format "Failed to send {} command over UART"

        event CommandForwardSuccess(cmd: string) severity activity high format "Command {} sent successfully"

        event DataReceived( data: U8, path: string) severity activity high format "Stored {} bytes of payload data to {}"

        event ByteReceived( byte: U8) severity activity low format "Received byte: {}"

        event UartReceived() severity activity low format "Received UART data"

        event BufferAllocationFailed(buffer_size: U32) severity warning high format "Failed to allocate buffer of size {}"

        event RawDataDump(byte0: U8, byte1: U8, byte2: U8, byte3: U8, byte4: U8, byte5: U8, byte6: U8, byte7: U8) severity activity low format "Raw: [{x} {x} {x} {x} {x} {x} {x} {x}]"

        @ Receives the desired command to forward through the payload UART
        sync input port commandIn: Drv.ByteStreamData

        @ Receives data from the UART, handles handshake protocol logic
        sync input port uartDataIn: Drv.ByteStreamData

        @ sends data to the UART, forwards commands and acknowledgement signals
        output port uartForward: Drv.ByteStreamSend

        @ Return RX buffers to UART driver (driver will deallocate to BufferManager)
        output port bufferReturn: Fw.BufferSend

        @ Sends data that is received by the uartDataIn port to the desired payload handler component
        output port uartDataOut: Drv.ByteStreamSend

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
