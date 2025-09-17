module Components {
    @ Manages the real time clock
    passive component RtcManager {

        @ SET_TIME command to set the time on the RTC
        @ Requirement RtcManager-001
        sync command SET_TIME opcode 0

        @ GET_TIME command to get the time from the RTC
        @ Requirement RtcManager-002
        sync command GET_TIME opcode 1

        ##############################################################################
        #### Uncomment the following examples to start customizing your component ####
        ##############################################################################

        # @ Example async command
        # async command COMMAND_NAME(param_name: U32)

        # @ Example telemetry counter
        # telemetry ExampleCounter: U64

        # @ Example event
        # event ExampleStateEvent(example_state: Fw.On) severity activity high id 0 format "State set to {}"
        event RTC_Set(status: bool) severity activity high id 0 format "Reset status: {}"

        event RTC_GetTime(year: U32, month: U32, day: U32, hour:U32, minute:U32, second:U32) severity activity high id 1 format "Time: {}/{}/{} at {}:{}:{}"

        event RTC_NotReady() severity warning high id 2 format "RTC not ready"

        # @ Example port: receiving calls from the rate group
        # sync input port run: Svc.Sched
        output port SetTime: Fw.Time

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

    }
}
