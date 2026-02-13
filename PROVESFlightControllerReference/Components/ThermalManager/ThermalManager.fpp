module Components {
    @ Thermal Manager Component for F Prime FSW framework.
    @ Orchestrates temperature sensor readings from 11 TMP112 sensors
    passive component ThermalManager {
        sync input port run: Svc.Sched

        @ The number of face temperature sensors
        constant numFaceTempSensors = 5

        @ Port for face temperature sensors
        output port faceTempGet: [numFaceTempSensors] Drv.temperatureGet

        @ Port for battery cell temperature sensors
        output port battCellTempGet: [4] Drv.temperatureGet

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
