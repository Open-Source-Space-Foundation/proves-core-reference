module Drv {
    @ RTC alarm manager component
    active component AlarmManager {

        # CMD to set alarms
        async command SET_ALARM(
            seconds: U32 @ "Seconds of the timestamp"
            nanoseconds: U32 @ "Nanoseconds part of the timestamp"
            ID: U32 @ "ID to indentify an alarm"
        )

        event ALARM_TRIGGER(ID: U32) \ 
            severity activity high \
            format "Alarm %u has triggered"

        # input sched port
        sync input port SchedIn: Svc.Sched

        # outgoing alarm port
        sync input port Alarm: Svc.Sched

        ##############################################################################
        #### Uncomment the following examples to start customizing your component ####
        ##############################################################################

        # @ Example async command
        # async command COMMAND_NAME(param_name: U32)

        # @ Example telemetry counter
        # telemetry ExampleCounter: U64

        # @ Example event
        # event ExampleStateEvent(example_state: Fw.On) severity activity high id 0 format "State set to {}"

        # @ Example port: receiving calls from the rate group
        # sync input port run: Svc.Sched

        # @ Example parameter
        # param PARAMETER_NAME: U32

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################

        @ Port for requesting the current time
        time get port timeCaller

        @ Enables command handling
        import Fw.Command

        @ Enables event handling
        import Fw.Event

        @ Enables telemetry channels handling
        import Fw.Channel

        @ Port to return the value of a parameter
        param get port prmGetOut

        @Port to set the value of a parameter
        param set port prmSetOut

    }
}