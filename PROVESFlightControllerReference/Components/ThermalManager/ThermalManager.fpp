module Components {
    @ Thermal Manager Component for F Prime FSW framework.
    @ Orchestrates temperature sensor readings from 11 TMP112 sensors
    passive component ThermalManager {
        ### Parameters ###
        @ Parameter for face temperature lower threshold in °C
        param FACE_TEMP_LOWER_THRESHOLD: F64 default -40.0 id 0

        @ Parameter for face temperature upper threshold in °C
        param FACE_TEMP_UPPER_THRESHOLD: F64 default 60.0 id 1

        @ Parameter for battery cell temperature lower threshold in °C
        param BATT_CELL_TEMP_LOWER_THRESHOLD: F64 default 5.0 id 2

        @ Parameter for battery cell temperature upper threshold in °C
        param BATT_CELL_TEMP_UPPER_THRESHOLD: F64 default 60.0 id 3

        sync input port run: Svc.Sched

        @ The number of face temperature sensors
        constant numFaceTempSensors = 5

        @ The number of battery cell temperature sensors
        constant numBattCellTempSensors = 4

        @ Port for face temperature sensors
        output port faceTempGet: [numFaceTempSensors] Drv.temperatureGet

        @ Port for battery cell temperature sensors
        output port battCellTempGet: [numBattCellTempSensors] Drv.temperatureGet

        @ Event for face temperature reading below threshold
        event FaceTemperatureBelowThreshold(sensorId: U32, temperature: F32) \
            severity warning low \
            format "Face temperature below threshold: Sensor {} at {} °C" \
            throttle 1

        @ Event for face temperature reading above threshold
        event FaceTemperatureAboveThreshold(sensorId: U32, temperature: F32) \
            severity warning low \
            format "Face temperature above threshold: Sensor {} at {} °C" \
            throttle 1

        @ Event for battery cell temperature reading below threshold
        event BatteryCellTemperatureBelowThreshold(sensorId: U32, temperature: F32) \
            severity warning low \
            format "Battery cell temperature below threshold: Sensor {} at {} °C" \
            throttle 1

        @ Event for battery cell temperature reading above threshold
        event BatteryCellTemperatureAboveThreshold(sensorId: U32, temperature: F32) \
            severity warning low \
            format "Battery cell temperature above threshold: Sensor {} at {} °C" \
            throttle 1

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for emitting telemetry
        telemetry port tlmOut

        @ Port for emitting events
        event port logOut

        @ Port for getting parameters
        param get port prmGetOut

        @ Port for setting parameters
        param set port prmSetOut

        @ Port for emitting text events
        text event port logTextOut

        @ Port for sending command registrations
        command reg port cmdRegOut

        @ Port for receiving commands
        command recv port cmdIn

        @ Port for sending command responses
        command resp port cmdResponseOut
    }
}
