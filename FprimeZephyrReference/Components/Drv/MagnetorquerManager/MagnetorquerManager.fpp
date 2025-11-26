module Drv {
    array InputArray = [5] bool;
    port SetMagnetorquers(
        value: InputArray @< Enabled/disabled vale for each face in the order x1, x2, y1, y2, z1
    )
    port SetDisabled()
}

module Drv {
    @ Component for F Prime FSW framework.
    passive component MagnetorquerManager {

        sync input port run: Svc.Sched

        @ Event for reporting DRV2605 not ready error
        event DeviceNotReady() severity warning high format "DRV2605 device not ready"

        @ Input port to set magnetorquer values
        sync input port SetMagnetorquers: SetMagnetorquers

        @ Input port to disable magnetorquers
        sync input port SetDisabled: SetDisabled

        @ Command to manually enable magnetorquers, for testing purposes
        sync command EnableMagnetorquers(InputArray: InputArray)

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

        @ Port to return the value of a parameter
        param get port prmGetOut

        @Port to set the value of a parameter
        param set port prmSetOut

    }
}
