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

        @ Command to reset the accumulated power consumption
        sync command RESET_TOTAL_POWER()

        @ Telemetry channel for accumulated power consumption in mWh
        telemetry TotalPowerConsumption: F32

        @ Event logged when total power consumption is reset
        event TotalPowerReset() \
            severity activity low \
            format "Total power consumption reset to 0 mWh"

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
