module Components {
    @ FS Format Component for F Prime FSW framework.
    passive component FsFormat {

        ### Commands ###

        @ Command to format the file system
        sync command FORMAT()

        ### Events ###

        @ Event for reporting File System formatted
        event FileSystemFormatted() severity activity high format "File System formatted successfully"

        @ Event for reporting File System format failed
        event FileSystemFormatFailed() severity warning high format "File System format failed"

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

    }
}
