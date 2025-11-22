module Zephyr {
    @ Update firmware images
    active component Updater {

        @ Set the image for the next boot in test boot mode
        async command NEXT_BOOT_TEST_IMAGE()

        @ Confirm this image for future boots
        async command CONFIRM_NEXT_BOOT_IMAGE()

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

    }
}
