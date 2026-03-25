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
    port AlarmSet(t: Fw.TimeValue) -> U32
    port AlarmCancel(ID: U16) -> U32
    port AlarmTriggered() -> U32
}

module Drv {
    @ Manages the real time clock
    passive component RtcManager {
        import Svc.Time

        ### COMMANDS ###

        # Alarm zephyr docs: https://docs.zephyrproject.org/latest/doxygen/html/group__rtc__interface.html#ga531c379c437a91df44a45e95063700e8
        # When setting an alarm we should generate an ID and return it as an event

        @ TIME_SET command to set the time on the RTC
        sync command TIME_SET(
            t: Drv.TimeData @< Set the time
        )

        @ ALARM_SET command to set an alarm on the RTC
        sync command ALARM_SET(
            t: Fw.TimeValue @< Time to set the alarm for
        )

        @ ALARM_CANCEL command to cancel any set alarms on the RTC
        sync command ALARM_CANCEL(
            ID: U16 @< ID of the alarm to cancel
        )

        @ ALARM_LIST command to list all set alarms on the RTC
        sync command ALARM_LIST()

        ### EVENTS ###

        @ DeviceNotReady event indicates that the RTC is not ready
        event DeviceNotReady() severity warning high id 0 format "RTC not ready" throttle 1

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

        @ AlarmSet event indicates that the alarm was set successfully
        event AlarmSet(
            ID: U16 @< ID of the set alarm
            t: Fw.TimeValue @< Time for the set alarm
        ) severity activity high id 11 format "Alarm set on RTC with ID {}, time: {}"

        @ AlarmNotSet event indicates that the alarm was not set successfully
        event AlarmNotSet(
            t: Fw.TimeValue @< Time for the alarm that was not set
            rc: U32 @< Return code from the RTC driver
        ) severity warning high id 12 format "Alarm not set on RTC for time: {}, return code: {}"

        @ AlarmTriggered event indicates that an alarm was triggered
        event AlarmTriggered(
            ID: U16 @< ID of the triggered alarm
        ) severity activity high id 13 format "Alarm with ID {} triggered"

        @ AlarmCanceled event indicates that an alarm was canceled successfully
        event AlarmCanceled(
            ID: U16 @< ID of the canceled alarm
        ) severity activity high id 14 format "Alarm with ID {} canceled"

        @ AlarmNotCanceled event indicates that an alarm was not canceled successfully
        event AlarmNotCanceled(
            ID: U16 @< ID of the alarm that was not canceled
            rc: U32 @< Return code from the RTC driver
        ) severity warning high id 15 format "Alarm with ID {} not canceled, return code: {}"

        ### PORTS ###

        @ Port for canceling running sequences when RTC time is set
        @ Connected to seqCancelIn ports of Command, Payload, and SafeMode sequencers
        output port cancelSequences: [3] Svc.CmdSeqCancel

        @ Port to indicate an alarm was triggered
        output port alarmTriggered: [1] Drv.AlarmTriggered

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
