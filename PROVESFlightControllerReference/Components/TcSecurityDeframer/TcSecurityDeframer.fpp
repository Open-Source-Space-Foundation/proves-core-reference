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

    @ A single HMAC authentication key slot
    struct AuthKeySlot {
        valid: bool    @< Whether this slot holds an active key
        spi: U16       @< Security Parameter Index selecting this key (CCSDS 355.0-B-2 key selector)
        key: [16] U8   @< Raw 128-bit HMAC key bytes
    }

    @ On-flash store of active authentication keys. Holds at most 2 keys so rotation can add a new
    @ key before removing the old one (see issue #220: keys must never be compiled into the image)
    array AuthKeyStore = [2] AuthKeySlot

    @ FPP shadow-enum representing Components::PacketKeyStore::ProvisionStatus
    enum KeyStoreProvisionStatus {
        Ok,           @< Command succeeded
        NotEmpty,     @< PROVISION_KEY was rejected because the store already has a key
        StoreFull,    @< ADD_KEY was rejected because the store already has 2 keys
        LastKey,      @< REMOVE_KEY was rejected because it would remove the last remaining key
        SpiNotFound,  @< REMOVE_KEY was rejected because no slot has the given SPI
        ParseKeyError,  @< The supplied key was not a valid 32-character hex string
        WriteError,   @< The updated store could not be written to the file system
        ImportError,  @< The updated key could not be imported into PSA
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

        @ Command to bootstrap a key onto a keyless satellite. Bypass-allowlisted so it can run
        @ before any key exists; rejected once the store already holds a key (use ADD_KEY to rotate)
        sync command PROVISION_KEY(spi: U16, key: string size 33)

        @ Command to add a second active key for rotation. Requires an authenticated frame. Fails
        @ if the store already has 2 keys
        sync command ADD_KEY(spi: U16, key: string size 33)

        @ Command to remove an active key by SPI. Requires an authenticated frame. Fails if it
        @ would drop the key count below 1
        sync command REMOVE_KEY(spi: U16)

        ### Telemetry ###

        @ Telemetry for the current sequence number, updated on each successfully authenticated packet
        telemetry CurrentSequenceNumber : U32

        @ Telemetry for the number of active keys in the key store, updated on configure() and every key store mutation
        telemetry ActiveKeyCount : U8

        ### Events ###

        @ SequenceNumberGet returns the current sequence number from the file system in response to a command
        event SequenceNumberGet(seq_num: U32) severity activity high id 6 format "Sequence number is {}"

        @SequenceNumberReadFailed indicates that there was an error reading the sequence number from the file system
        event SequenceNumberReadFailed(status: Os.FileStatus) severity warning high id 14 format "Failed to read sequence number, error: {}" throttle 2

        @ SequenceNumberSet indicates that the sequence number was set to a specified value through a command
        event SequenceNumberSet(seq_num: U32) severity activity high id 7 format "Sequence number set to {}"

        @ SequenceNumberWriteFailed indicates that there was an error writing the sequence number to file
        event SequenceNumberWriteFailed(status: Os.FileStatus) severity warning high id 8 format "Failed to write sequence number, error: {}" throttle 2

        @ SequenceNumberInvalid indicates that a received packet had a sequence number that was outside of the acceptable window
        event SequenceNumberInvalid(packet_seq_num: U32, seq_num: U32, window: U32) severity warning high id 2 format "Sequence number less than last accepted or out of window: Received={}, LastAccepted={}, Window={}" throttle 2

        @ AuthenticationFailed indicates that a received packet failed authentication
        event AuthenticationFailed(auth_status: PacketAuthenticatorStatus, rc: I32) severity warning high id 1 format "Authentication failed: Status={}, PSA Return Code={}" throttle 2

        @ ParsingFailed indicates that there was an error parsing a received packet
        event ParsingFailed(parse_status: PacketParserStatus) severity warning high id 3 format "Parsing failed: {}" throttle 2

        @ SpiInvalid indicates that a received packet had an invalid SPI value
        event SpiInvalid(packet_spi: U32) severity warning high id 4 format "SPI invalid: Received={}" throttle 2

        @ KeyStoreReadFailed indicates that there was an error reading the key store from the file system
        event KeyStoreReadFailed(status: Os.FileStatus) severity warning high id 16 format "Failed to read key store, error: {}" throttle 2

        @ KeyStoreWriteFailed indicates that there was an error writing the key store to the file system
        event KeyStoreWriteFailed(status: Os.FileStatus) severity warning high id 17 format "Failed to write key store, error: {}" throttle 2

        @ KeyProvisioned indicates PROVISION_KEY succeeded
        event KeyProvisioned(spi: U16) severity activity high id 9 format "Key provisioned for SPI={}"

        @ KeyProvisionFailed indicates PROVISION_KEY was rejected or failed
        event KeyProvisionFailed(status: KeyStoreProvisionStatus) severity warning high id 10 format "Key provisioning failed: {}" throttle 2

        @ KeyAdded indicates ADD_KEY succeeded
        event KeyAdded(spi: U16) severity activity high id 11 format "Key added for SPI={}"

        @ KeyAddFailed indicates ADD_KEY was rejected or failed
        event KeyAddFailed(status: KeyStoreProvisionStatus) severity warning high id 12 format "Key add failed: {}" throttle 2

        @ KeyRemoved indicates REMOVE_KEY succeeded
        event KeyRemoved(spi: U16) severity activity high id 13 format "Key removed for SPI={}"

        @ KeyRemoveFailed indicates REMOVE_KEY was rejected or failed
        event KeyRemoveFailed(status: KeyStoreProvisionStatus) severity warning high id 15 format "Key remove failed: {}" throttle 2

        ### Parameters ###

        @ Parameter for the sequence numbers window size, used to prevent replay attacks. The window allows no reuse of previous sequence numbers but allows for new sequence numbers to be accepted within the window size
        param SEQ_NUM_WINDOW : U32 default 50000

        @ Parameter for the file path where the current sequence number is stored
        param SEQ_NUM_FILE_PATH : string default "/keys/sequence_number.bin"

        @ Parameter for the file path where the HMAC authentication key store is stored
        param KEY_STORE_FILE_PATH : string default "/keys/authkeys.bin"

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
