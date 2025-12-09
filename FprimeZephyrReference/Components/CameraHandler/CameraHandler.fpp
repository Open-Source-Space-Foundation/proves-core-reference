module Components {
    @ Active component that handles camera-specific payload protocol processing and file saving
    @ Receives data from PayloadCom, parses image protocol, saves files
    passive component CameraHandler {

        # Commands
        @ Type in "snap" to capture an image
        sync command TAKE_IMAGE()

        @ Camera Ping
        sync command PING()

        @ Send command to camera via PayloadCom
        sync command SEND_COMMAND(cmd: string)

        # Events for command handling
        event CommandError(cmd: string) severity warning high format "Failed to send {} command"

        event CommandSuccess(cmd: string) severity activity high format "Command {} sent successfully"

        # Events for image transfer (clean, minimal output)
        @ Emitted when image transfer begins
        event ImageTransferStarted($size: U32) severity activity high format "Image transfer started, expecting {} bytes"

        @ Emitted at 25%, 50%, 75% progress milestones
        event ImageTransferProgress(percent: U8, received: U32, expected: U32) severity activity high format "Image transfer {}% complete ({}/{} bytes)"

        @ Emitted when image transfer completes successfully
        event ImageTransferComplete(path: string, $size: U32) severity activity high format "Image saved: {} ({} bytes)"

        event FailedCommandCurrentlyReceiving() severity warning low format "Cannot send command while image is receiving!"

        event PongReceived() severity activity high format "Ping Received"

        event BadPongReceived() severity warning high format "Ping Received when we did not expect it!"

        event FileWriteError() severity warning high format "File write error occurred during image transfer"

        event FileReadError() severity warning high format "File read error occurred during image transfer"

        # Telemetry for debugging image transfer state
        @ Number of bytes received so far in current image transfer
        telemetry BytesReceived: U32

        @ Expected total size of image being received
        telemetry ExpectedSize: U32

        @ Whether currently receiving image data
        telemetry IsReceiving: bool

        @ Whether file is currently open for writing
        telemetry FileOpen: bool

        @ Total number of file errors encountered
        telemetry FileErrorCount: U32

        @ Total number of images successfully saved
        telemetry ImagesSaved: U32

        # Ports
        @ Sends command to PayloadCom to be forwarded over UART
        output port commandOut: Drv.ByteStreamData

        @ Receives data from PayloadCom, handles image protocol parsing and file saving
        sync input port dataIn: Drv.ByteStreamData

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
