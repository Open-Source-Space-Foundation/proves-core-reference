module Drv {
    port VoltageGet -> F64
    port CurrentGet -> F64
    port PowerGet -> F64
}

    module Drv {
    @ Manager for Ina219 device
    passive component Ina219Manager {

        #Ports
        @ Port to read the voltage in volts
        sync input port voltageGet: VoltageGet

        @ Port to read the current in amps
        sync input port currentGet: CurrentGet

        @ Port to read the power in watts
        sync input port powerGet: PowerGet

        # Telemetry channels

        @ Telemetry channel for voltage in volts
        telemetry Voltage: F64

        @ Telemetry channel for current in amps
        telemetry Current: F64

        @ Telemetry channel for power in watts
        telemetry Power: F64

        @ Event for reporting INA219 not ready error
        event DeviceNotReady() severity warning high format "INA219 device not ready" throttle 5


        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

        @ Port for sending telemetry channels to downlink
        telemetry port tlmOut

    }
}
