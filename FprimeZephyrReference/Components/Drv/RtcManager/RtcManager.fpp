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
            seconds: U32 @< Seconds since epoch
            useconds: U32 @< Microseconds
        ) severity activity high id 3 format "Time set on RTC, previous time: {}.{}"

        @ TimeNotSet event indicates that the time was not set successfully
        event TimeNotSet() severity warning high id 4 format "Time not set on RTC"

        @ YearValidationFailed event indicates that the provided year is invalid
        event YearValidationFailed(
            year: U32 @< The invalid year
        ) severity warning high id 5 format "Provided year is invalid should be >= 1900: {}"

        @ MonthValidationFailed event indicates that the provided month is invalid
        event MonthValidationFailed(
            month: U32 @< The invalid month
        ) severity warning high id 6 format "Provided month is invalid should be in [1, 12]: {}"

        @ DayValidationFailed event indicates that the provided day is invalid
        event DayValidationFailed(
            day: U32 @< The invalid day
        ) severity warning high id 7 format "Provided day is invalid should be in [1, 31]: {}"

        @ HourValidationFailed event indicates that the provided hour is invalid
        event HourValidationFailed(
            hour: U32 @< The invalid hour
        ) severity warning high id 8 format "Provided hour is invalid should be in [0, 23]: {}"

        @ MinuteValidationFailed event indicates that the provided minute is invalid
        event MinuteValidationFailed(
            minute: U32 @< The invalid minute
        ) severity warning high id 9 format "Provided minute is invalid should be in [0, 59]: {}"

        @ SecondValidationFailed event indicates that the provided second is invalid
        event SecondValidationFailed(
            second: U32 @< The invalid second
        ) severity warning high id 10 format "Provided second is invalid should be in [0, 59]: {}"

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
