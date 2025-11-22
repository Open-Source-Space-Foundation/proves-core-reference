module Drv {
    array InputArray = [5] I32;
    port SetMagnetorquers(
        value: InputArray @< Amp value for each face in the order x1, x2, y1, y2, z1
    )
}

module Drv {
    @ Component for F Prime FSW framework.
    passive component MagnetorquerManager {

        sync input port run: Svc.Sched


        @ Event for reporting DRV2605 not ready error
        event DeviceNotReady() severity warning high format "DRV2605 device not ready"

        @ Event to report an invalid face index passed in
        event InvalidFaceIndex() severity warning low format "The faceIdx should be between 0-4"

        event RetResult(ret: U8) severity activity high format "Result of operation: {}"

        @ Command to set enabled state
        sync command SET_ENABLED(enable: bool)

        @ Start DRV2605 playback on a device with effect #47 on a specific face
        @ faceIdx: index of the face to actuate (valid range: 0..4)
        sync command START_PLAYBACK_TEST(faceIdx: U8)

        @ Start DRV2605 playback on a device with effect #50 on a specific face
        @ faceIdx: index of the face to actuate (valid range: 0..4)
        sync command START_PLAYBACK_TEST2(faceIdx: U8)

        @ Input port to set magnetorquer values
        sync input port SetMagnetorquers: SetMagnetorquers

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
