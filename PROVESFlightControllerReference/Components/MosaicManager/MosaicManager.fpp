module Components {
    @ Passive component that receives gamma ray detector data from the MOSAIC
    @ payload over UART and stores it on disk as F Prime data products.
    @ MOSAIC streams ASCII lines of the form "ADC=<raw>,MV=<millivolts>\n".
    @ The manager only listens; it never sends commands to the payload.
    passive component MosaicManager {

        # ----------------------------------------------------------------------
        # Types
        # ----------------------------------------------------------------------

        @ A single gamma ray detector sample parsed from a MOSAIC CSV line
        struct MosaicSample {
            @ Raw 12-bit ADC code (0-4095)
            adc: U16
            @ ADC reading converted to millivolts by the payload
            millivolts: U16
        }

        # ----------------------------------------------------------------------
        # Data products
        # ----------------------------------------------------------------------

        @ A single gamma ray detector sample
        product record SampleRecord: MosaicSample id 0

        @ Container holding a batch of gamma ray detector samples
        product container GammaData id 0 default priority 10

        # ----------------------------------------------------------------------
        # Commands
        # ----------------------------------------------------------------------

        @ Start recording received samples into data products (default on boot)
        sync command START_RECORDING()

        @ Stop recording; flushes any partially filled data product to disk
        sync command STOP_RECORDING()

        @ Flush the partially filled data product to disk now
        sync command FLUSH()

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ Recording was started
        event RecordingStarted() severity activity high format "MOSAIC sample recording started"

        @ Recording was stopped
        event RecordingStopped() severity activity high format "MOSAIC sample recording stopped"

        @ A data product container was completed and sent to be written to disk
        event DataProductSent(records: U32) \
            severity activity high \
            format "MOSAIC data product sent with {} samples"

        @ Failed to get a data product buffer; samples will be dropped
        event DpMemoryFail() \
            severity warning high \
            format "Failed to acquire a MOSAIC data product buffer" \
            throttle 5

        @ A received line could not be parsed as a MOSAIC sample
        event LineParseError() \
            severity warning low \
            format "Failed to parse a MOSAIC line" \
            throttle 5

        @ A serialization error occurred while recording a sample
        event RecordSerializeError() \
            severity warning high \
            format "Failed to serialize a MOSAIC sample record" \
            throttle 5

        @ UART receive reported a bad status
        event UartReceiveError() \
            severity warning low \
            format "MOSAIC UART receive error" \
            throttle 5

        # ----------------------------------------------------------------------
        # Telemetry
        # ----------------------------------------------------------------------

        @ Whether samples are currently being recorded to data products
        telemetry Recording: bool

        @ Total samples recorded to data products
        telemetry SamplesRecorded: U32

        @ Total data product containers sent to be written to disk
        telemetry ProductsSent: U32

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
        sync input port dataIn: Drv.ByteStreamData

        @ Returns receive buffers to the UART driver
        output port bufferReturn: Fw.BufferSend

        @ Rate group input for periodic telemetry and data product flush
        sync input port run: Svc.Sched

        @ Data product get port (synchronous buffer request)
        product get port productGetOut

        @ Data product send port
        product send port productSendOut

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
