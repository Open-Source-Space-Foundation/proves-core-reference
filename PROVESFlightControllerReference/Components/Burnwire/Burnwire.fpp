module Components {
    @ Turns Burnwire on and off
    passive component Burnwire {

        @ START_BURNWIRE turns on the burnwire
        sync command START_BURNWIRE(
        )

        @ STOP_BURNWIRE turns on the burnwire
        sync command STOP_BURNWIRE(
        )

        event SetBurnwireState(burnwire_state: Fw.On) \
            severity activity high \
            format "Burnwire State: {}"

        event SafetyTimerStatus(burnwire_state: Fw.On) \
            severity activity high\
            format "Safety Timer State: {} "

        event SafetyTimerState(burnwire_status: U32) \
            severity activity high\
            format "Safety Timer Will Burn For: {} Seconds"

        event BurnwireEndCount(end_count: U32) \
            severity activity low \
            format "Burnwire Burned for {} Seconds"

        @ Port getting start signal
        sync input port burnStart: Fw.Signal

        @ Port getting stop signal
        sync input port burnStop: Fw.Signal

        @ Input Port to get the rate group
        sync input port schedIn: Svc.Sched

        @ Port sending calls to the GPIO driver to stop and start the burnwire
        output port gpioSet: [2] Drv.GpioWrite

        # @ SAFETY_TIMER parameter is the maximum time that the burn component will run
        param SAFETY_TIMER: U32 default 10

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
