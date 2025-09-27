# Port definition
module Drv {
    port MagneticFieldGet -> MagneticField
}

# Component definition
module Drv {
    @ LIS2MDL Manager Component for F Prime FSW framework.
    passive component Lis2mdlManager {
        @ Port to read the current magnetic field in gauss.
        sync input port magneticFieldGet: MagneticFieldGet

        @ Event for reporting LSM6DSO not ready error
        event DeviceNotReady() severity warning high format "LIS2MDL device not ready" throttle 5

        @ Telemetry channel for magnetic field in gauss
        telemetry MagneticField: MagneticField


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
