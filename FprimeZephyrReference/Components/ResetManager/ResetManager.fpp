module Components {
    @ Component to soft reset the satellite
    passive component ResetManager {

        @ Command to initiate a cold reset
        sync command COLD_RESET()

        @ Command to initiate a warm reset
        sync command WARM_RESET()

        @ Event indicating that a cold reset has been initiated
        event INITIATE_COLD_RESET() severity activity high id 0 format "Initiating cold reset"

        @ Event indicating that a warm reset has been initiated
        event INITIATE_WARM_RESET() severity activity high id 1 format "Initiating warm reset"

        @ Port to invoke a cold reset
        sync input port coldReset: Fw.Signal

        @ Port to invoke a warm reset
        sync input port warmReset: Fw.Signal

        @ Port to notify ModeManager before reboot (sets clean shutdown flag)
        output port prepareForReboot: Fw.Signal

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
