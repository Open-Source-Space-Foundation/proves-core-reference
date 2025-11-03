module Components {
    @ Manager for ina219 device for power monitoring
    passive component PowerMonitor {
        sync input port run: Svc.Sched

        @ Port for sending voltageGet calls to the INA219 Driver
        output port voltageGet: Drv.VoltageGet

        @ Port for sending currentGet calls to the INA219 Driver
        output port currentGet: Drv.CurrentGet

        @ Port for sending powerGet calls to the INA219 Driver
        output port powerGet: Drv.PowerGet

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

    }
}