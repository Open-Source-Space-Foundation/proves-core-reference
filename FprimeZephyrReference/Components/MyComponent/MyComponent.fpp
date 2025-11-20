

module Components {
    @ Component for F Prime FSW framework.
    passive component MyComponent {

        ##############################################################################
        #### Uncomment the following examples to start customizing your component ####
        ##############################################################################

        @ Port receiving calls from the rate group
        sync input port run: Svc.Sched

        # @ Import Communication Interface
        # import Svc.Com

        @ Command for testing
        sync command FOO()

        @ Reset Radio Module
        sync command RESET()

        @ SPI Output Port
        output port spiSend: Drv.SpiReadWrite

        @ Radio Module Reset GPIO
        output port resetSend: Drv.GpioWrite

        @ S-Band radio busy read (caller port)
        output port gpioBusyRead: Drv.GpioRead

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
