module Components {
    @ Interactions between Radio Amateurs and Sats
    passive component AmateurRadio {

        ##############################################################################
        #### Component-specific ports, commands, events, and telemetry            ####
        ##############################################################################

        @ Command to tell a random satellite-themed joke
        sync command TELL_JOKE()

        @ Event with the joke text
        event JokeTold(joke: string size 140) \
            severity activity high id 1 \
            format "{}"

        @ Amount of tags this component has goten
        telemetry count_names: U32

        # @ Example port: receiving calls from the rate group
        # sync input port run: Svc.Sched

        # @ Example parameter
        # param PARAMETER_NAME: U32

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
