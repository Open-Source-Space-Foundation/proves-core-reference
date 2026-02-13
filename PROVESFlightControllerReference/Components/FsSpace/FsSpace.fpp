module Components {
    @ Read free space
    passive component FsSpace {

        ##############################################################################
        #### Uncomment the following examples to start customizing your component ####
        ##############################################################################

        @ Free disk space telemetry channel
        telemetry FreeSpace: FwSizeType

        @ Total disk space telemetry channel
        telemetry TotalSpace: FwSizeType

        sync input port run: Svc.Sched

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending telemetry channels to downlink
        telemetry port tlmOut

    }
}
