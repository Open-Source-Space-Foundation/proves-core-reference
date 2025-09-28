module Components {
    @ Manages the real time clock
    passive component RtcManager {
        @ SET_TIME command to set the time on the RTC
        sync command SET_TIME(
            t: Drv.TimeData @< Set the time
        ) opcode 0

        @ GET_TIME command to get the time from the RTC
        sync command GET_TIME opcode 1

        ##############################################################################
        #### Uncomment the following examples to start customizing your component ####
        ##############################################################################

        @ Event to log the time retrieved from the Rv3028Manager in ISO 8601 format
        event GetTime(t: string) severity activity high id 1 format "{}"

        @ Output port to set the time on the Rv3028Manager
        output port timeSet: Drv.TimeSet

        @ Output port to get the time from the Rv3028Manager
        output port timeGet: Drv.TimeGet

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
