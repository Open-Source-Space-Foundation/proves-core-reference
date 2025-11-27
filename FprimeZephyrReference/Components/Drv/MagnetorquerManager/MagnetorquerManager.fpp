module Drv {
    # Making the array 10 elements in case of any future expansion
    array InputArray = [10] InputStruct
    struct InputStruct {
        key: string
        value: bool
    }

    port SetMagnetorquers(
        value: InputArray @< Enabled/disabled values for each face
    )
    port SetDisabled()
}

module Drv {
    @ Component for F Prime FSW framework.
    passive component MagnetorquerManager {

        sync input port run: Svc.Sched

        @ Event for reporting DRV2605 not ready error
        event DeviceNotReady(face: string) severity warning high format "DRV2605 device on face {} not ready"

        @ Event for reporting DRV2605 initialization error
        event DeviceNotInitialized(face: string) severity warning high format "DRV2605 device on face {} not initialized"

        @ Event for reporting that an invalid face was specified
        event InvalidFace(face: string) severity warning high format "Invalid face specified: {}"

        @ Input port to set magnetorquer values
        sync input port SetMagnetorquers: SetMagnetorquers

        @ Input port to disable magnetorquers
        sync input port SetDisabled: SetDisabled

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
