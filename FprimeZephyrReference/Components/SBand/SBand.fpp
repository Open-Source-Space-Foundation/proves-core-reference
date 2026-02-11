

module Components {

    @ SX1280 Spreading Factor / Data Rate
    enum SBandDataRate : U8 {
        SF_5 = 5
        SF_6 = 6
        SF_7 = 7
        SF_8 = 8
        SF_9 = 9
        SF_10 = 10
        SF_11 = 11
        SF_12 = 12
    }

    @ SX1280 Coding Rate
    enum SBandCodingRate : U8 {
        CR_4_5 = 5
        CR_4_6 = 6
        CR_4_7 = 7
        CR_4_8 = 8
    }

    @ SX1280 LoRa Bandwidth (kHz)
    enum SBandBandwidth : U8 {
        BW_203_125_KHZ = 0
        BW_406_25_KHZ = 1
        BW_812_5_KHZ = 2
        BW_1625_KHZ = 3
    }

    @ Transmit state for controlling radio transmission
    enum SBandTransmitState : U8 {
        ENABLED
        DISABLED
    }

    @ Component for F Prime FSW framework.
    active component SBand {

        ##############################################################################
        #### Uncomment the following examples to start customizing your component ####
        ##############################################################################

        @ Port receiving calls from the rate group
        sync input port run: Svc.Sched

        @ Internal port for deferred RX processing
        internal port deferredRxHandler() priority 10

        @ Internal port for deferred TX processing
        internal port deferredTxHandler(
            data: Fw.Buffer
            context: ComCfg.FrameContext
        ) priority 10

        @ Internal port for deferred TRANSMIT command processing
        internal port deferredTransmitCmd(
            enabled: SBandTransmitState
        ) priority 10

        @ Import Communication Interface
        import Svc.Com

        @ Import the allocation interface
        import Svc.BufferAllocation

        @ SPI Output Port
        output port spiSend: Drv.SpiReadWrite

        @ Radio Module Reset GPIO
        output port resetSend: Drv.GpioWrite

        @ S-Band TX Enable GPIO (separate from reset)
        output port txEnable: Drv.GpioWrite

        @ S-Band RX Enable GPIO
        output port rxEnable: Drv.GpioWrite

        @ S-Band IRQ Line
        output port getIRQLine: Drv.GpioRead

        @ Event to indicate RadioLib call failure
        event RadioLibFailed(error: I16) severity warning high \
            format "SBand RadioLib call failed, error: {}" throttle 2

        @ Event to indicate allocation failure
        event AllocationFailed(allocation_size: FwSizeType) severity warning high \
            format "Failed to allocate buffer of: {} bytes" throttle 2

        @ Event to indicate radio not configured
        event RadioNotConfigured() severity warning high \
            format "Radio not configured, operation ignored" throttle 3

        @ Event when DATA_RATE parameter is set
        event DataRateParamSet(value: SBandDataRate) severity activity high \
            format "DATA_RATE parameter set to {}."

        @ Event when CODING_RATE parameter is set
        event CodingRateParamSet(value: SBandCodingRate) severity activity high \
            format "CODING_RATE parameter set to {}."

        @ Event when BANDWIDTH_TX parameter is set
        event BandwidthTxParamSet(value: SBandBandwidth) severity activity high \
            format "BANDWIDTH_TX parameter set to {}."

        @ Event when BANDWIDTH_RX parameter is set
        event BandwidthRxParamSet(value: SBandBandwidth) severity activity high \
            format "BANDWIDTH_RX parameter set to {}."

        @ Last received RSSI (if available)
        telemetry LastRssi: F32 update on change

        @ Last received SNR (if available)
        telemetry LastSnr: F32 update on change

        ###############################################################################
        # Parameters                                                                   #
        ###############################################################################

        @ Data rate / spreading factor
        param DATA_RATE: SBandDataRate default SBandDataRate.SF_7

        @ Coding rate: data bits/total bits (including checksum/hamming)
        param CODING_RATE: SBandCodingRate default SBandCodingRate.CR_4_5

        @ Bandwidth for transmission
        param BANDWIDTH_TX: SBandBandwidth default SBandBandwidth.BW_406_25_KHZ

        @ Bandwidth for reception
        param BANDWIDTH_RX: SBandBandwidth default SBandBandwidth.BW_406_25_KHZ

        @ DATA_RATE parameter value
        telemetry DataRateParam: SBandDataRate

        @ CODING_RATE parameter value
        telemetry CodingRateParam: SBandCodingRate

        @ BANDWIDTH_TX parameter value
        telemetry BandwidthTxParam: SBandBandwidth

        @ BANDWIDTH_RX parameter value
        telemetry BandwidthRxParam: SBandBandwidth

        ###############################################################################
        # Commands                                                                     #
        ###############################################################################

        @ Start/stop transmission on the S-Band module
        sync command TRANSMIT(enabled: SBandTransmitState)

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
