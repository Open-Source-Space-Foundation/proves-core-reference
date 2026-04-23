module Components {
    @ FPP shadow-enum representing Components::PacketAuthenticator::Status
    enum PacketAuthenticatorStatus {
        Authenticated,    @< Packet is authenticated
        InitError,        @< There was an error initializing the authentication process
        ParseKeyError,    @< There was an error parsing the key from storage
        ImportKeyError,   @< There was an error importing the authentication key
        VerifyError,      @< The packet HMAC did not match the expected value
        DestroyKeyError,  @< There was an error destroying the authentication key
    }

    @ FPP shadow-enum representing Components::PacketParser::Status
    enum PacketParserStatus {
        Ok,                        @< Packet was successfully parsed
        SpiParseError,             @< SPI could not be parsed from packet
        SequenceNumberParseError,  @< Sequence number could not be parsed from packet
        OpCodeParseError,          @< OpCode could not be parsed from packet
        HmacParseError,            @< HMAC could not be parsed from packet
    }

    # @ FPP shadow-struct representing Components::PacketValidator::Status
    # enum PacketValidatorStatus {
    #     Valid,                      @< Packet is valid
    #     Bypass,                     @< Packet OpCode is allowed to bypass authentication
    #     SpiInvalid,                 @< SPI did not match expected value
    #     SequenceNumberOutOfWindow,  @< Sequence number is outside the acceptable window
    # }

    @ Component placed between the radio component and the cdh. It ensures that any commands are authenticated before they are acted on. Some commands and messages do not require being authenticated
    passive component PacketProcessor {

        ### Commands ###

        @ Command to get the current sequence number
        sync command GET_SEQ_NUM()

        @ Command to set the current sequence number
        sync command SET_SEQ_NUM(seq_num: U32)

        ### Telemetry ###

        @ Telemetry for the current sequence number, updated on each successfully authenticated packet
        telemetry CurrentSequenceNumber : U32

        @ Telemetry for the count of bypassed packets, updated on each packet that bypasses authentication based on OpCode
        telemetry BypassPacketsCount : U32

        @ Telemetry for the count of rejected packets, updated on each packet that fails authentication
        telemetry RejectedPacketsCount : U32

        ### Events ###

        @ SequenceNumberGet returns the current sequence number from the file system in response to a command
        event SequenceNumberGet(seq_num: U32) severity activity high id 6 format "Sequence number is {}"

        @SequenceNumberReadFailed indicates that there was an error reading the sequence number from the file system
        event SequenceNumberReadFailed(status: Os.FileStatus) severity warning high id 14 format "Failed to read sequence number, error: {}"

        @ SequenceNumberSet indicates that the sequence number was set to a specified value through a command
        event SequenceNumberSet(seq_num: U32) severity activity high id 7 format "Sequence number set to {}" throttle 2

        @ SequenceNumberWriteFailed indicates that there was an error writing the sequence number to file
        event SequenceNumberWriteFailed(status: Os.FileStatus) severity warning high id 8 format "Failed to write sequence number, error: {}" throttle 2

        @ SequenceNumberOutOfWindow indicates that a received packet had a sequence number that was outside of the acceptable window
        event SequenceNumberOutOfWindow(expected: U32, window: U32) severity warning high id 2 format "Sequence number out of window: Expected={}, Window={}" throttle 2

        @ AuthenticationFailed indicates that a received packet failed authentication
        event AuthenticationFailed(auth_status: PacketAuthenticatorStatus, rc: I32) severity warning high id 1 format "Authentication failed: Status={}, PSA Return Code={}" throttle 2

        @ ParsingFailed indicates that there was an error parsing a received packet
        event ParsingFailed(parse_status: PacketParserStatus) severity warning high id 3 format "Parsing failed: {}" throttle 2

        @ SpiInvalid indicates that a received packet had an invalid SPI value
        event SpiInvalid(received: U32) severity warning high id 4 format "SPI invalid: Received {}" throttle 2

        ### Parameters ###

        @ Parameter for the sequence numbers window size, used to prevent replay attacks. The window allows no reuse of previous sequence numbers but allows for new sequence numbers to be accepted within the window size
        param SEQ_NUM_WINDOW : U32 default 50000

        @ Parameter for the file path where the current sequence number is stored
        param SEQ_NUM_FILE_PATH : string default "//sequence_number.txt"

        ### Ports ###

        @ Port receiving Space Packets from TcDeframer
        guarded input port dataIn: Svc.ComDataWithContext

        @ Port receiving back ownership of buffers sent to dataOut (SpacePacketDeframer)
        sync input port dataReturnIn: Svc.ComDataWithContext

        @ Port forwarding authenticated or non-authenticated packets to SpacePacketDeframer
        output port dataOut: Svc.ComDataWithContext

        @ Port returning ownership of invalid/unauthorized packets back to upstream component (TcDeframer)
        output port dataReturnOut: Svc.ComDataWithContext

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
