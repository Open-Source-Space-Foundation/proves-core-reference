module Components {
    @ Component to blink an LED driven by a rate group
    active component Led {

        @ Command to turn on or off the blinking LED
        async command BLINKING_ON_OFF(
                onOff: Fw.On @< Indicates whether the blinking should be on or off
        )

        @ Command to set the LED blink interval
        async command SET_BLINK_INTERVAL(
                interval: U32 @< Blink interval in rate group ticks
        )

        @ Telemetry channel to report blinking state.
        telemetry BlinkingState: Fw.On

        @ Telemetry channel counting LED transitions
        telemetry LedTransitions: U64

        @ Reports the state we set to blinking.
        event SetBlinkingState($state: Fw.On) \
            severity activity high \
            format "Set blinking state to {}."

        @ Event logged when the LED turns on or off
        event LedState(onOff: Fw.On) \
            severity activity low \
            format "LED is {}"

        @ Event logged when the LED blink interval is updated
        event BlinkIntervalSet(interval: U32) \
            severity activity high \
            format "LED blink interval set to {}"

        @ Port receiving calls from the rate group
        async input port run: Svc.Sched

        @ Port sending calls to the GPIO driver
        output port gpioSet: Drv.GpioWrite

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