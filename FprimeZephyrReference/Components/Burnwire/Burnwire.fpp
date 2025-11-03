module Components {
    @ Turns Burnwire on and off
    passive component Burnwire {

        @ START_BURNWIRE turns on the burnwire
        sync command START_BURNWIRE(
        )

        @ STOP_BURNWIRE turns on the burnwire
        sync command STOP_BURNWIRE(
        )

        @ BurnwireState event logs the current state of the burnwire
        event BurnwireState(burnwire_state: Fw.On) \
            severity activity high \
            format "Burnwire State: {}"

        @ TimeoutParamInvalid event logs when the TIMEOUT parameter is invalid
        event TimeoutParamInvalid() \
            severity warning low \
            format "TIMEOUT parameter invalid, cancelling burn"

        @ BurnwireAlreadyOn event logs when a start command is received while the burnwire is already on
        event BurnwireAlreadyOn() \
            severity warning high \
            format "Burnwire already ON, ignoring start command"

        @ Port getting start signal
        sync input port burnStart: Fw.Signal

        @ Port getting stop signal
        sync input port burnStop: Fw.Signal

        @ Input Port to get the rate group
        sync input port schedIn: Svc.Sched

        @ Port sending calls to the GPIO driver to stop and start the burnwire
        output port gpioSet: [2] Drv.GpioWrite

        # @ TIMEOUT parameter is the maximum time that the burn component will run
        param TIMEOUT: U32 default 10

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
