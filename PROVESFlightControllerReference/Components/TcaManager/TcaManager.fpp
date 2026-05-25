module Components {
    @ Component to pet the tcaManager, driven by a rate group
    passive component TcaManager {
        @ Event when the mux reset is toggled
        event MuxResetToggled() \
            severity activity high \
            format "Mux reset toggled"

        @ Port to indicate a TCA error has occurred
        sync input port tcaError: Fw.Signal

        @ Port sending calls to reset the mux
        output port muxReset: Drv.GpioWrite

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

    }
}
