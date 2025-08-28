module Components {
    @ Component for F Prime FSW framework.
    passive component Imu {
        sync input port run: Svc.Sched

        telemetry ImuCalls: U64

        event ImuTestEvent() \
            severity activity high \
            format "WOAH WHATS HAPPENING IS THIS WORKING HELLO"

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

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
