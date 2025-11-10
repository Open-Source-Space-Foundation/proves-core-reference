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
        # Telemetry Channels                                                          #
        ###############################################################################
        @ Face 0 temperature in degrees Celsius
        telemetry Face0Temperature: F64

        @ Face 1 temperature in degrees Celsius
        telemetry Face1Temperature: F64

        @ Face 2 temperature in degrees Celsius
        telemetry Face2Temperature: F64

        @ Face 3 temperature in degrees Celsius
        telemetry Face3Temperature: F64

        @ Face 4 temperature in degrees Celsius
        telemetry Face4Temperature: F64

        @ Face 5 temperature in degrees Celsius
        telemetry Face5Temperature: F64

        @ Top temperature in degrees Celsius
        telemetry TopTemperature: F64

        @ Battery cell 1 temperature in degrees Celsius
        telemetry BattCell1Temperature: F64

        @ Battery cell 2 temperature in degrees Celsius
        telemetry BattCell2Temperature: F64

        @ Battery cell 3 temperature in degrees Celsius
        telemetry BattCell3Temperature: F64

        @ Battery cell 4 temperature in degrees Celsius
        telemetry BattCell4Temperature: F64

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
