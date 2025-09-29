# Type definition
module Drv {
    struct TimeData {
        Year: U32 @< Year value.
        Month: U32 @< Month value.
        Day: U32 @< Day value.
        Hour: U32 @< Hour value.
        Minute: U32 @< Minute value.
        Second: U32 @< Second value.
    }
}

# Port definition
module Drv {
    port TimeSet(t: TimeData)
    port TimeGet -> U32
}

module Drv {
    @ Manages the real time clock
    passive component RtcManager {
        import Svc.Time

        @ TIME_SET command to set the time on the RTC
        sync command TIME_SET(
            t: Drv.TimeData @< Set the time
        ) opcode 0

        ##############################################################################
        #### Uncomment the following examples to start customizing your component ####
        ##############################################################################

        @ DeviceNotReady event indicates that the RTC is not ready
        event DeviceNotReady() severity warning high id 0 format "RTC not ready" throttle 5

        @ TimeSet event indicates that the time was set successfully
        event TimeSet(
            pt: U32 @< POSIX time in seconds
            u: U32 @< Microseconds
        ) severity activity high id 3 format "Time set on RTC, previous time: {}.{}"

        @ TimeNotSet event indicates that the time was not set successfully
        event TimeNotSet() severity warning high id 4 format "Time not set on RTC"

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
