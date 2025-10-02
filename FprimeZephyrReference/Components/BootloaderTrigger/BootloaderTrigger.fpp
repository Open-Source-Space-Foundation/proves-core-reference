module Components {
    @ Used to trigger bootloader mode to install board firmware for integration testing. (DO NOT USE IN SPACE VERY BAD!!!!)
    passive component BootloaderTrigger {

        @ Restarts board and puts it in bootloader mode. (Only should be used for integration testing)
        sync command TRIGGER_BOOTLOADER(
        )

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

    }
}
