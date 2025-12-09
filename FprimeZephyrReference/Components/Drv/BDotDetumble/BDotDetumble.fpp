module Drv {
    port DipoleMomentGet(
        currMagField: MagneticField
        prevMagField: MagneticField
    ) -> DipoleMoment
}

module Drv {
    @ Component for F Prime FSW framework.
    passive component BDotDetumble {

        sync input port dipoleMomentGet: DipoleMomentGet

        telemetry DipoleMoment: DipoleMoment

        event DipoleThing(x: F64, y: F64, z: F64) severity warning low format "X: {}, Y: {}, Z: {}"
        event BruhMoment severity warning low format "BRUH MOMENT"

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

        @ Port to return the value of a parameter
        param get port prmGetOut

        @Port to set the value of a parameter
        param set port prmSetOut

    }
}
