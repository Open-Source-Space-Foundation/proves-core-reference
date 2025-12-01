module Components {
    @ Attitude Determination and Control Component for F Prime FSW framework.
    passive component ADCS {
        sync input port run: Svc.Sched

        @ Port for face light sensors
        output port visibleLightGet: [6] Drv.lightGet

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for emitting telemetry
        telemetry port tlmOut

        @ Port for emitting events
        event port logOut

        @ Port for emitting text events
        text event port logTextOut
    }
}
