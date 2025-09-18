module Components {
    @ Magnetic field reading in gauss
    struct MagneticField {
        x: F64
        y: F64
        z: F64
    }

    # Port definitions
    port MagneticFieldRead -> MagneticField

    @ Component for F Prime FSW framework.
    passive component Lis2mdlDriver {

        sync input port getMagneticField: MagneticFieldRead

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

    }
}
