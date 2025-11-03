module Components {
    @ Manager for ina219 device for power monitoring
    passive component PowerMonitor {
        sync input port run: Svc.Sched

        @ Port for sending voltageGet calls to the System INA219 Driver
        output port sysVoltageGet: Drv.VoltageGet

        @ Port for sending currentGet calls to the System INA219 Driver
        output port sysCurrentGet: Drv.CurrentGet

        @ Port for sending powerGet calls to the System INA219 Driver
        output port sysPowerGet: Drv.PowerGet

        @ Port for sending voltageGet calls to the Solar Panel INA219 Driver
        output port solVoltageGet: Drv.VoltageGet

        @ Port for sending currentGet calls to the Solar Panel INA219 Driver
        output port solCurrentGet: Drv.CurrentGet

        @ Port for sending powerGet calls to the Solar Panel INA219 Driver
        output port solPowerGet: Drv.PowerGet

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

    }
}
