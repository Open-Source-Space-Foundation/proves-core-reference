module Components {
    @ Thermal Manager Component for F Prime FSW framework.
    @ Orchestrates temperature sensor readings from 11 TMP112 sensors
    passive component ThermalManager {
        sync input port run: Svc.Sched

        # Output ports to  instances
        @ Port to return the state of the TCA
        output port tcaHealthGet: Components.HealthGet

        # XY Boards

        @ Port to return the state of mux channel 0
        output port muxChannel0HealthGet: Components.HealthGet

        @ Port to return the state of the load switch for face 0
        output port face0LoadSwitchStateGet: Components.loadSwitchStateGet

        @ Port for face 0 temperature sensor initialization
        output port face0Init: Fw.SuccessCondition

        @ Port for face 0 temperature sensor
        output port face0TempGet: Drv.temperatureGet

        @ Port to return the state of mux channel 1
        output port muxChannel1HealthGet: Components.HealthGet

        @ Port for face 1 load switch state
        output port face1LoadSwitchStateGet: Components.loadSwitchStateGet

        @ Port for face 1 temperature sensor initialization
        output port face1Init: Fw.SuccessCondition

        @ Port for face 1 temperature sensor
        output port face1TempGet: Drv.temperatureGet

        @ Port to return the state of mux channel 2
        output port muxChannel2HealthGet: Components.HealthGet

        @ Port for face 2 load switch state
        output port face2LoadSwitchStateGet: Components.loadSwitchStateGet

        @ Port for face 2 temperature sensor initialization
        output port face2Init: Fw.SuccessCondition

        @ Port for face 2 temperature sensor
        output port face2TempGet: Drv.temperatureGet

        @ Port to return the state of mux channel 3
        output port muxChannel3HealthGet: Components.HealthGet

        @ Port for face 3 load switch state
        output port face3LoadSwitchStateGet: Components.loadSwitchStateGet

        @ Port for face 3 temperature sensor initialization
        output port face3Init: Fw.SuccessCondition

        @ Port for face 3 temperature sensor
        output port face3TempGet: Drv.temperatureGet

        # Z- Board

        @ Port to return the state of mux channel 5
        output port muxChannel5HealthGet: Components.HealthGet

        @ Port for face 5 load switch state
        output port face5LoadSwitchStateGet: Components.loadSwitchStateGet

        @ Port for face 5 temperature sensor initialization
        output port face5Init: Fw.SuccessCondition

        @ Port for face 5 temperature sensor
        output port face5TempGet: Drv.temperatureGet

        # Battery Cell

        @ Port to return the state of mux channel 4
        output port muxChannel4HealthGet: Components.HealthGet

        @ Port for battery cell 1 temperature sensor initialization
        output port battCell1Init: Fw.SuccessCondition

        @ Port for battery cell 1 temperature sensor
        output port battCell1TempGet: Drv.temperatureGet

        @ Port for battery cell 2 temperature sensor initialization
        output port battCell2Init: Fw.SuccessCondition

        @ Port for battery cell 2 temperature sensor
        output port battCell2TempGet: Drv.temperatureGet

        @ Port for battery cell 3 temperature sensor initialization
        output port battCell3Init: Fw.SuccessCondition

        @ Port for battery cell 3 temperature sensor
        output port battCell3TempGet: Drv.temperatureGet

        @ Port for battery cell 4 temperature sensor initialization
        output port battCell4Init: Fw.SuccessCondition

        @ Port for battery cell 4 temperature sensor
        output port battCell4TempGet: Drv.temperatureGet

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
