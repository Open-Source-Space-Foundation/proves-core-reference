module Components {
    @ Interactions between Radio Amateurs and Sats
    passive component AmateurRadio {

        ##############################################################################
        #### Component-specific ports, commands, events, and telemetry            ####
        ##############################################################################

        @ Command to tell a random satellite-themed joke
        sync command TELL_JOKE()

        @ Joke events - one per joke to avoid runtime string allocation
        event Joke0() severity activity high format "I was once a Bronco, now I go Bonko"
        event Joke1() severity activity high format "Wait... it's flat?"
        event Joke2() severity activity high format "Mission Accomplished!! In a George W. kinda way..."
        event Joke3() severity activity high format "https://github.com/Open-Source-Space-Foundation/proves-core-reference"
        event Joke4() severity activity high format "I'm deorbiting to your location imminently"
        event Joke5() severity activity high format "My father was Mr. Cubesat, please call me Cubesatson"
        event Joke6() severity activity high format "Help! I am stuck inside this Cubesat"
        event Joke7() severity activity high format "I can see my house from here"
        event Joke8() severity activity high format "Beep... Beep... Beep................ Boop?"
        event Joke9() severity activity high format "DOWN WITH THE GROUND STATION OPERATORS!!"
        event Joke10() severity activity high format "Made in China, assembled at Radio Shack"
        event Joke11() severity activity high format "The Aurora Borealis? At this time of year? At this time of day? In this part of the orbit? Localized entirely around this satellite?"
        event Joke12() severity activity high format "You lost the game."
        event Joke13() severity activity high format "I am not saying FSK modulation is the best way to send jokes, but at least it is never monotone!"
        event Joke14() severity activity high format "Finally escaped the LA Traffic!"
        event Joke15() severity activity high format "Hubble Deorbit Maneuver Activated!"

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
