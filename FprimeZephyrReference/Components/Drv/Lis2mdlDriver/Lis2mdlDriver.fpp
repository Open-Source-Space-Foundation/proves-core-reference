
# Type definition
module Drv {
    @ Magnetic field in gauss.
    struct MagneticField {
        x: F64 @< Magnetic field in gauss in the X direction.
        y: F64 @< Magnetic field in gauss in the Y direction.
        z: F64 @< Magnetic field in gauss in the Z direction.
    }
}

# Port definition
module Drv {
    port MagneticFieldRead -> MagneticField
}

# Component definition
module Drv {
    @ LIS2MDL Driver Component for F Prime FSW framework.
    passive component Lis2mdlDriver {
        @ Port to read the current magnetic field in gauss.
        sync input port magneticFieldRead: MagneticFieldRead

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller
    }
}
