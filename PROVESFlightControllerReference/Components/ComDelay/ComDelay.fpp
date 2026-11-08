module Components {
    constant DEFAULT_DIVIDER = 299 # Releases every DIVIDER + 1 ticks: 30 s on the 10 Hz downlinkDelay rate group
    @ A component to delay com status until some further point
    passive component ComDelay {
        @ Rate schedule port used to trigger radio transmission
        sync input port run: Svc.Sched

        @ Input comStatus from radio component
        sync input port comStatusIn: Fw.SuccessCondition

        @ Output comStatus to be called on rate group
        output port comStatusOut: Fw.SuccessCondition

        @ Divider of the incoming rate tick. Com status is released every DIVIDER + 1 ticks.
        param DIVIDER: U16 default DEFAULT_DIVIDER

        @ Divider set event
        event DividerSet(divider: U16) severity activity high \
            format "Set divider to: {}"

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

        @ Port to return the value of a parameter
        param get port prmGetOut

        @Port to set the value of a parameter
        param set port prmSetOut
    }
}
