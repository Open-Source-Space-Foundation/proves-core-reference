# Type definition
module Drv {
    struct TimeData {
        Year: U32 @< Year value.
        Month: U32 @< Month value.
        Day: U32 @< Day value.
        Hour: U32 @< Hour value.
        Minute: U32 @< Minute value.
    }
}

# Port definition
module Drv {
    port TimeSet(t: TimeData)
    port TimeRead -> U32
}

module Drv {
    @ Manages the real time clock
    passive component Rv3028Manager {
        import Svc.Time

        @ timeSet port to set the time on the RTC
        @ Requirement Rv3028Manager-001
        sync input port timeSet: TimeSet

        @ timeRead port to get the time from the RTC
        @ Requirement Rv3028Manager-002
        sync input port timeRead: TimeRead

        ##############################################################################
        #### Uncomment the following examples to start customizing your component ####
        ##############################################################################

        @ DeviceNotReady event indicates that the RV3028 is not ready
        event DeviceNotReady() severity warning high id 0 format "RV3028 not ready" throttle 5

        @ InvalidTime event indicates that the RV3028 returned an invalid time
        event InvalidTime() severity warning high id 1 format "RV3028 returned invalid time" throttle 5

        @ TimeGet event indicates that the time was read successfully
        event TimeGet(
            t: U32 @< POSIX time read from RTC
        ) severity activity high id 2 format "RV3028 Time: {}"

        @ TimeSet event indicates that the time was set successfully
        event TimeSet() severity activity high id 3 format "Time set on RV3028"

        @ TimeNotSet event indicates that the time was not set successfully
        event TimeNotSet() severity warning high id 4 format "Time not set on RV3028"

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut
    }
}
