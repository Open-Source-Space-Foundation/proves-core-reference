module Components {
    @ Power Monitor Manager
    passive component PowerMonitor {
        sync input port run: Svc.Sched

        @ Port for sending voltageGet calls to the System Driver
        output port sysVoltageGet: Drv.VoltageGet

        @ Port for sending currentGet calls to the System Driver
        output port sysCurrentGet: Drv.CurrentGet

        @ Port for sending powerGet calls to the System Driver
        output port sysPowerGet: Drv.PowerGet

        @ Port for sending voltageGet calls to the Solar Panel Driver
        output port solVoltageGet: Drv.VoltageGet

        @ Port for sending currentGet calls to the Solar Panel Driver
        output port solCurrentGet: Drv.CurrentGet

        @ Port for sending powerGet calls to the Solar Panel Driver
        output port solPowerGet: Drv.PowerGet

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

    }
}
