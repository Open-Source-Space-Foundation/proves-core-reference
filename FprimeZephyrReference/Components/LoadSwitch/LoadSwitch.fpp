module Components {
    port loadSwitchStateGet -> Fw.On
}

module Components {
    @ A generic load switch for controlling power to components
    passive component LoadSwitch {

        @ Command to turn the load switch on
        sync command TURN_ON()

        @ Command to turn the load switch off
        sync command TURN_OFF()

        @ Telemetry channel for load switch state
        telemetry IsOn: Fw.On

        @ Event for reporting load switch state change
        event StatusChanged($state: Fw.On) severity activity high id 1 format "Load switch state changed to {}"

        @ Port sending calls to the GPIO driver
        output port gpioSet: Drv.GpioWrite

        @ Port sending calls to the GPIO driver to read state
        output port gpioGet: Drv.GpioRead

        @ Input port to turn on the load switch (called by other components)
        sync input port turnOn: Fw.Signal

        @ Input port to turn off the load switch (called by other components)
        sync input port turnOff: Fw.Signal

        @ Input port to get the state of the load switch (called by other components)
        sync input port loadSwitchStateGet: loadSwitchStateGet

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
