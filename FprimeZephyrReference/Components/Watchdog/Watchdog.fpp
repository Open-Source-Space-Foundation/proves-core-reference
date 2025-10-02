module Components {
    @ Component to pet the watchdog, driven by a rate group
    passive component Watchdog {
        @ Command to start the watchdog
        sync command START_WATCHDOG(
        )

        @ Command to stop the watchdog
        sync command STOP_WATCHDOG(
        )

        @ Telemetry channel counting watchdog pet transitions
        telemetry WatchdogTransitions: U32

        @ Event logged when the watchdog is started
        event WatchdogStart() \
            severity activity high \
            format "Watchdog started"

        @ Event logged when the watchdog is stopped
        event WatchdogStop() \
            severity activity high \
            format "Watchdog stopped"

        @ Port receiving calls from the rate group
        sync input port run: Svc.Sched

        @ Port to start the watchdog
        sync input port start: Fw.Signal

        @ Port to stop the watchdog
        sync input port stop: Fw.Signal

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
