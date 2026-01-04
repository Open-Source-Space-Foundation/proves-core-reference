module Components {
    @ Interactions between Radio Amateurs and Sats
    passive component AmateurRadio {

        ##############################################################################
        #### Component-specific ports, commands, events, and telemetry            ####
        ##############################################################################

        @ The satelie will repeat back the radio name
        sync command Repeat_Name(radio_name: string size 64)

        @ Command to tell a random satellite-themed joke
        sync command TELL_JOKE()

        @ The satelie will broadcast back the radio name and the sat name
        event ReapeatingName(radio_name: string size 64, sat_name: string size 64) \
            severity activity high id 0 \
            format "Repeating name: radio={}, sat={}"

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
