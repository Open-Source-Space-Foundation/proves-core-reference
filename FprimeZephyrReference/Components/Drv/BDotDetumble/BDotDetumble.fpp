module Drv {
    port DipoleMomentGet(ref condition: Fw.Success) -> DipoleMoment
}

module Drv {
    @ Component for F Prime FSW framework.
    passive component BDotDetumble {

        ### Ports ###

        @ Input port to get the current dipole moment
        sync input port dipoleMomentGet: DipoleMomentGet

        @ Port for reading the magnetic field from the magnetometer
        output port magneticFieldGet: Drv.MagneticFieldGet

        ### Telemetry ###

        @ Telemetry for the gain used in B-Dot algorithm
        telemetry Gain: F64

        ### Parameters ###

        @ Gain used for B-Dot algorithm
        param Gain: F64 default 1.0

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
