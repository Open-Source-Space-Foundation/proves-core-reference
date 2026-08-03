module Components {
    @ Passive component that receives gamma ray detector data from the MOSAIC
    @ payload over UART and stores it on disk under the /mosaic directory.
    @ MOSAIC streams ASCII lines of the form "ADC=<raw>,MV=<millivolts>\n".
    @ The manager only listens; it never sends commands to the payload.
    @
    @ Ports and commands are guarded, not sync: dataIn is driven from the 10Hz
    @ rate group thread, run from the 1Hz rate group thread, and the commands
    @ from the command dispatcher thread. All three mutate the open sample file
    @ and its counters, so they must serialize on the component mutex --
    @ otherwise a STOP_RECORDING or a stale-file flush can close the file out
    @ from under an in-flight sample write.
    passive component MosaicManager {

        # ----------------------------------------------------------------------
        # Commands
        # ----------------------------------------------------------------------

        @ Start recording received samples to the filesystem (default on boot)
        guarded command START_RECORDING()

        @ Stop recording; flushes and closes the current file
        guarded command STOP_RECORDING()

        @ Flush the current file to disk and close it now
        guarded command FLUSH()

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ Recording was started
        event RecordingStarted() severity activity high format "MOSAIC sample recording started"

        @ Recording was stopped
        event RecordingStopped() severity activity high format "MOSAIC sample recording stopped"

        @ A sample file was completed and closed
        event SampleFileClosed(fileName: string size 32, records: U32) \
            severity activity high \
            format "MOSAIC sample file {} closed with {} samples"

        @ Failed to open a new sample file on the filesystem; samples will be dropped
        event FileOpenError(fileName: string size 32, status: U32) \
            severity warning high \
            format "Failed to open MOSAIC sample file {} (status {})" \
            throttle 5

        @ A write to the current sample file failed
        event FileWriteError(status: U32) \
            severity warning high \
            format "Failed to write MOSAIC sample record (status {})" \
            throttle 5

        @ A received line could not be parsed as a MOSAIC sample
        event LineParseError() \
            severity warning low \
            format "Failed to parse a MOSAIC line" \
            throttle 5

        @ UART receive reported a bad status
        event UartReceiveError() \
            severity warning low \
            format "MOSAIC UART receive error" \
            throttle 5

        # ----------------------------------------------------------------------
        # Telemetry
        # ----------------------------------------------------------------------

        @ Whether samples are currently being recorded to the filesystem
        telemetry Recording: bool

        @ Total samples recorded to the filesystem
        telemetry SamplesRecorded: U32

        @ Total sample files closed and ready for downlink
        telemetry FilesWritten: U32

        @ Total lines that failed to parse
        telemetry ParseErrors: U32

        @ Most recent raw ADC reading
        telemetry LatestAdc: U16

        @ Most recent reading in millivolts
        telemetry LatestMillivolts: U16

        # ----------------------------------------------------------------------
        # Ports
        # ----------------------------------------------------------------------

        @ Receives raw byte stream from the MOSAIC UART driver
        guarded input port dataIn: Drv.ByteStreamData

        @ Returns receive buffers to the UART driver
        output port bufferReturn: Fw.BufferSend

        @ Rate group input for periodic telemetry and file flush
        guarded input port run: Svc.Sched

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
