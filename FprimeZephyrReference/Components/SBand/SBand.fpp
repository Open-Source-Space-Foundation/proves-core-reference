

module Components {
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

        @ Last received RSSI (if available)
        telemetry LastRssi: F32 update on change

        @ Last received SNR (if available)
        telemetry LastSnr: F32 update on change

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
