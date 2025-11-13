module Components {
    @ String type for hash values
    type HashString = string size 128

    @ String type for error reasons
    type ReasonString = string size 256

    @ String type for APID lists
    type ApidString = string size 256

    @ Component placed between the radio component and the cdh. It ensures that any commands are authenticated before they are acted on. Some commands and messages do not require being authenticated
    passive component Authenticate {

        ##############################################################################
        #### Uncomment the following examples to start customizing your component ####
        ##############################################################################

        # @ Example async command
        # async command COMMAND_NAME(param_name: U32)

        sync command GET_SEQ_NUM()

        sync command SET_SEQ_NUM(seq_num: U32)

        sync command GET_KEY_FROM_SPI(spi: U32)

        # @ Example telemetry counter
        # telemetry ExampleCounter: U64

        telemetry AuthenticatedPacketsCount : U64

        telemetry RejectedPacketsCount : U64

        telemetry CurrentSequenceNumber : U32

        # @ Events for packet authentication

        event ValidHash(apid: U32, spi: U32, seqNum: U32) severity activity high id 0 format "Authenticated packet: APID={}, SPI={}, SeqNum={}"

        event InvalidHash(apid: U32, spi: U32, seqNum: U32) severity warning high id 1 format "Authentication failed: APID={}, SPI={}, SeqNum={}"

        event SequenceNumberOutOfWindow(spi: U32, expected: U32, window: U32) severity warning high id 2 format "Sequence number out of window: SPI={}, Expected={}, Window={}"

        event InvalidSPI(spi: U32) severity warning high id 3 format "Invalid SPI received: SPI={}"

        event APIDMismatch(spi: U32, packetApid: U32) severity warning high id 4 format "APID mismatch: SPI={}, Packet APID={}"

        event EmitSequenceNumber(seq_num: U32) severity activity high id 6 format "The current sequence number is {}"

        event SetSequenceNumberSuccess(seq_num: U32, status: bool) severity activity high id 7 format "sequence number has been set to {}: {}"

        event InvalidAuthenticationType(authType: U32) severity warning high id 8 format "Invalid authentication type {}"

        event EmitSpiKey(key: HashString, authType: HashString) severity activity high id 9 format "SPI key is {} type is {}"

        # @ Ports for packet authentication

        @ Port receiving Space Packets from TcDeframer
        guarded input port dataIn: Svc.ComDataWithContext

        @ Port forwarding authenticated or non-authenticated packets to SpacePacketDeframer
        output port dataOut: Svc.ComDataWithContext

        @ Port returning ownership of invalid/unauthorized packets back to upstream component (TcDeframer)
        output port dataReturnOut: Svc.ComDataWithContext

        @ Port receiving back ownership of buffers sent to dataOut (SpacePacketDeframer)
        sync input port dataReturnIn: Svc.ComDataWithContext

        # @ Example parameter
        # param PARAMETER_NAME: U32

        param SEQ_NUM_WINDOW : U32 default 50


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
