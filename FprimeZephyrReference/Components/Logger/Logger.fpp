module Components {
    port SerialGet(serialData: Fw.ComBuffer)
}

module Components {
    @ Log telemtry data to onboard SD storage
    passive component Logger {

        @ Port for taking in serial data
        sync input port serialGet: SerialGet
       

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