module Components {
    @ Active component that handles camera-specific payload protocol processing and file saving
    @ Receives data from PayloadCom, parses image protocol, saves files
    passive component CameraHandler {

        # Commands
        @ Type in "snap" to capture an image
        sync command TAKE_IMAGE()

        @ Send command to camera via PayloadCom
        sync command SEND_COMMAND(cmd: string)

        # Events for protocol processing and file handling
        event CommandError(cmd: string) severity warning high format "Failed to send {} command"

        event CommandSuccess(cmd: string) severity activity high format "Command {} sent successfully"

        event DataReceived(data: U8, path: string) severity activity high format "Stored {} bytes of payload data to {}"

        event ImageHeaderReceived() severity activity low format "Received image header"

        event ImageSizeExtracted(imageSize: U32) severity activity high format "Image size from header: {} bytes"

        event ImageTransferProgress(received: U32, expected: U32) severity activity low format "Transfer progress: {}/{} bytes"

        event ImageDataOverflow() severity warning high format "Image data overflow - buffer full"

        event ProtocolBufferDebug(bufSize: U32, firstByte: U8) severity activity low format "Protocol buffer: {} bytes, first: 0x{x}"

        event HeaderParseAttempt(bufSize: U32) severity activity low format "Attempting header parse with {} bytes"

        event RawDataDump(byte0: U8, byte1: U8, byte2: U8, byte3: U8, byte4: U8, byte5: U8, byte6: U8, byte7: U8) severity activity low format "Raw: [{x} {x} {x} {x} {x} {x} {x} {x}]"

        # Ports
        @ Sends command to PayloadCom to be forwarded over UART
        output port commandOut: Drv.ByteStreamData

        @ Receives data from PayloadCom, handles image protocol parsing and file saving
        sync input port dataIn: Drv.ByteStreamData

        @ Return buffers after processing
        output port bufferReturn: Fw.BufferSend

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