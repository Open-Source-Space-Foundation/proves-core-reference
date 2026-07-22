module Components {
    @ FPP shadow-enum representing Components::PacketAuthenticator::AuthenticationStatus
    enum PacketAuthenticatorStatus {
        Authenticated,  @< Packet is authenticated
        VerifyError,    @< The packet HMAC did not match the expected value
    }

    @ FPP shadow-enum representing Components::PacketParser::Status
    enum PacketParserStatus {
        Ok,                        @< Packet was successfully parsed
        SpiParseError,             @< SPI could not be parsed from packet
        SequenceNumberParseError,  @< Sequence number could not be parsed from packet
        MacParseError,             @< MAC could not be parsed from packet
    }

    @ Component placed between the TcDeframer and SpacePacketDeframer components. It
    @ implements the TC ProcessSecurity flow of CCSDS 355.0-B-2: parse the Security
    @ Header and Trailer, validate the SPI and anti-replay sequence number, and verify
    @ the MAC. The verification result is recorded in the frame context
    @ (authenticated flag); policy enforcement (reject or bypass) is owned downstream
    @ by the router.
    passive component TcSecurityDeframer {

        ### Commands ###

        @ Command to get the current sequence number
        sync command GET_SEQ_NUM()

        @ Command to set the current sequence number
        sync command SET_SEQ_NUM(seq_num: U32)

        ### Telemetry ###

        @ Telemetry for the current sequence number, updated on each successfully authenticated packet
        telemetry CurrentSequenceNumber : U32

        ### Events ###

        @ SequenceNumberGet returns the current sequence number from the file system in response to a command
        event SequenceNumberGet(seq_num: U32) severity activity high id 6 format "Sequence number is {}"

        @SequenceNumberReadFailed indicates that there was an error reading the sequence number from the file system
        event SequenceNumberReadFailed(status: Os.FileStatus) severity warning high id 14 format "Failed to read sequence number, error: {}" throttle 2

        @ SequenceNumberSet indicates that the sequence number was set to a specified value through a command
        event SequenceNumberSet(seq_num: U32) severity activity high id 7 format "Sequence number set to {}"

        @ SequenceNumberWriteFailed indicates that there was an error writing the sequence number to file
        event SequenceNumberWriteFailed(status: Os.FileStatus) severity warning high id 8 format "Failed to write sequence number, error: {}" throttle 2

        @ SequenceNumberPersistFailed indicates that the write-ahead high-water persist (issue #461)
        @ failed to reach disk. The in-RAM high-water mark is deliberately NOT advanced in this case
        @ (that would reopen the anti-replay window on a subsequent reboot) -- a retry is scheduled
        @ after a bounded number of further accepted frames instead of retrying on every single one.
        event SequenceNumberPersistFailed(status: Os.FileStatus, accepted_seq_num: U32) severity warning high id 10 format "Write-ahead sequence-number persist failed, error: {} (accepted seq {}); retrying after a bounded backoff, not every frame" throttle 2

        @ SequenceNumberInvalid indicates that a received packet had a sequence number that was outside of the acceptable window
        event SequenceNumberInvalid(packet_seq_num: U32, seq_num: U32, window: U32) severity warning high id 2 format "Sequence number less than last accepted or out of window: Received={}, LastAccepted={}, Window={}" throttle 2

        @ SequenceNumberRecordInvalid indicates that the persisted sequence-number record failed its
        @ torn-write validation (checksum mismatch) on boot, or could not be read for another reason.
        @ The runtime sequence number falls back to 0 (same as a genuine first boot) so command
        @ capability is never blocked on this outcome; ground may issue SET_SEQ_NUM to fast-forward
        @ past any previously-used sequence numbers if the real last-used value is known.
        event SequenceNumberRecordInvalid(stored_value: U32) severity warning high id 9 format "Persisted sequence-number record failed validation (raw value read: {}); falling back to 0 -- use SET_SEQ_NUM to fast-forward if needed"

        @ AuthenticationFailed indicates that a received packet failed authentication
        event AuthenticationFailed(auth_status: PacketAuthenticatorStatus, rc: I32) severity warning high id 1 format "Authentication failed: Status={}, PSA Return Code={}" throttle 2

        @ ParsingFailed indicates that there was an error parsing a received packet
        event ParsingFailed(parse_status: PacketParserStatus) severity warning high id 3 format "Parsing failed: {}" throttle 2

        @ SpiInvalid indicates that a received packet had an invalid SPI value
        event SpiInvalid(packet_spi: U32) severity warning high id 4 format "SPI invalid: Received={}" throttle 2

        ### Parameters ###

        @ Parameter for the sequence numbers window size, used to prevent replay attacks. The window allows no reuse of previous sequence numbers but allows for new sequence numbers to be accepted within the window size
        param SEQ_NUM_WINDOW : U32 default 50000

        @ Parameter for the file path where the current sequence number is stored
        param SEQ_NUM_FILE_PATH : string default "//sequence_number.txt"

        ### Ports ###

        @ Port receiving frames from TcDeframer: [Security Header | Data Field | Security Trailer]
        guarded input port dataIn: Svc.ComDataWithContext

        @ Port forwarding the Data Field to SpacePacketDeframer with the authenticated flag set in the context
        output port dataOut: Svc.ComDataWithContext

        @ Port returning ownership of structurally invalid frames back to TcDeframer
        output port dataReturnOut: Svc.ComDataWithContext

        @ Port receiving back ownership of buffers sent on dataOut
        sync input port dataReturnIn: Svc.ComDataWithContext

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
