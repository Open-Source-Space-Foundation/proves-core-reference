module Components {
    @ Thermal Manager Component for F Prime FSW framework.
    @ Orchestrates temperature sensor readings from 11 TMP112 sensors
    passive component ThermalManager {
        sync input port run: Svc.Sched

        # Output ports to TMP112Manager instances
        @ Port for face 0 temperature sensor
        output port face0TempGet: Drv.AmbientTemperatureGet

        @ Port for face 1 temperature sensor
        output port face1TempGet: Drv.AmbientTemperatureGet

        @ Port for face 2 temperature sensor
        output port face2TempGet: Drv.AmbientTemperatureGet

        @ Port for face 3 temperature sensor
        output port face3TempGet: Drv.AmbientTemperatureGet

        @ Port for face 4 temperature sensor
        output port face4TempGet: Drv.AmbientTemperatureGet

        @ Port for face 5 temperature sensor
        output port face5TempGet: Drv.AmbientTemperatureGet

        @ Port for top temperature sensor
        output port topTempGet: Drv.AmbientTemperatureGet

        @ Port for battery cell 1 temperature sensor
        output port battCell1TempGet: Drv.AmbientTemperatureGet

        @ Port for battery cell 2 temperature sensor
        output port battCell2TempGet: Drv.AmbientTemperatureGet

        @ Port for battery cell 3 temperature sensor
        output port battCell3TempGet: Drv.AmbientTemperatureGet

        @ Port for battery cell 4 temperature sensor
        output port battCell4TempGet: Drv.AmbientTemperatureGet

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller
    }
}
