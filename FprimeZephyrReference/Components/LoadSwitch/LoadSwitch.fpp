module Components {
    @ A generic load switch for controlling power to components
    passive component LoadSwitch {

        # One async command/port is required for active components
        # This should be overridden by the developers with a useful command/port

        ##############################################################################
        #### Uncomment the following examples to start customizing your component ####
        ##############################################################################

        # @ Example async command
        # async command COMMAND_NAME(param_name: U32)
        sync command TURN_ON()
        sync command TURN_OFF()

        # @ Example telemetry counter
        # telemetry ExampleCounter: U64
        telemetry IsOn: Fw.On

        # @ Example event
        # event ExampleStateEvent(example_state: Fw.On) severity activity high id 0 format "State set to {}"
        event StatusChanged($state: Fw.On) severity activity high id 1 format "Load switch state changed to {}"

        # @ Example port: receiving calls from the rate group
        # sync input port run: Svc.Sched
        #output port Status: Drv.GpioRead
        #We will not be putting a Drv.GpioRead port here, we are using the Gpio Driver component which has this already!

        @ Port sending calls to the GPIO driver
        output port gpioSet: Drv.GpioWrite


        # Input that will be used by other components if they want to force a reset
        # (off and on again) of the load switch
        sync input port Reset: Fw.Signal

        @ Input port to turn on the load switch (called by other components)
        sync input port turnOn: Fw.Signal

        @ Input port to turn off the load switch (called by other components)
        sync input port turnOff: Fw.Signal

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

        @ Port to return the value of a parameter
        param get port prmGetOut

        @Port to set the value of a parameter
        param set port prmSetOut

    }
}
