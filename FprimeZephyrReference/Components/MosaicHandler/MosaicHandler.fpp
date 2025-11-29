module Components {
    @ Active component that handles camera-specific payload protocol processing and file saving
    @ Receives data from PayloadCom, parses mosaic data protocol, saves files
    passive component MosaicHandler {

        # Commands
        @ Type in "gamma_begin" to capture a gamma reading
        sync command TAKE_GAMMA_READING()

        @ Send command to mosaic via PayloadCom
        sync command SEND_COMMAND(cmd: string)

        # Events for protocol processing and file handling
        event CommandError(cmd: string) severity warning high format "Failed to send {} command"

        event CommandSuccess(cmd: string) severity activity high format "Command {} sent successfully"

        event DataReceived(data: U8, path: string) severity activity high format "Stored {} bytes of payload data to {}"

        event GammaReadingHeaderReceived() severity activity low format "Received gamma reading header"

        event GammaReadingSizeExtracted(gammaReadingSize: U32) severity activity high format "Gamma reading size from header: {} bytes"

        event GammaReadingTransferProgress(received: U32, expected: U32) severity activity low format "Transfer progress: {}/{} bytes"

        event ChunkWritten(chunkSize: U32) severity activity low format "Wrote {} bytes to file"

        event GammaReadingDataOverflow() severity warning high format "Gamma reading data overflow - buffer full"

        event ProtocolBufferDebug(bufSize: U32, firstByte: U8) severity activity low format "Protocol buffer: {} bytes, first: 0x{x}"

        event HeaderParseAttempt(bufSize: U32) severity activity low format "Attempting header parse with {} bytes"

        event RawDataDump(byte0: U8, byte1: U8, byte2: U8, byte3: U8, byte4: U8, byte5: U8, byte6: U8, byte7: U8) severity activity low format "Raw: [{x} {x} {x} {x} {x} {x} {x} {x}]"

        # Ports
        @ Sends command to PayloadCom to be forwarded over UART
        output port commandOut: Drv.ByteStreamData

        @ Receives data from PayloadCom, handles mosaic protocol parsing and file saving
        sync input port dataIn: Drv.ByteStreamData

        # Telemetry channels
        @ Telemetry channel for gamma radiation reading in millivolts
        telemetry GammaRadiationReading: F64

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

    }
}
