// ======================================================================
// \title  RtcManager.cpp
// \brief  cpp file for RtcManager component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/Drv/RtcManager/RtcManager.hpp"

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

RtcManager ::RtcManager(const char* const compName)
    : RtcManagerComponentBase(compName),
      m_dev(nullptr),
      m_lock({}),
      m_discipline(),
      m_RtcNotDisciplinedThrottle(false),
      m_disciplineReadFaults(0),
      m_disciplineRejects(0) {
    // alarm time initialization
    memset(&this->m_alarm_time, 0, sizeof(struct rtc_time));
}

RtcManager ::~RtcManager() {}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

void RtcManager ::configure(const struct device* dev) {
    this->m_dev = dev;

    // match to timedata this is constant after being updated here, do not change
    int rc = rtc_alarm_get_supported_fields(this->m_dev, 0, &this->m_curr_mask);
    if (rc != 0) {
        // log failure
        this->log_WARNING_HI_AlarmHardwareError(0, rc);
    }

    // Clear a stale alarm flag (AF) left over from a prior boot. AF survives a
    // processor reset; if left set, registering the first alarm callback in
    // ALARM_SET would trigger it immediately.
    if (device_is_ready(this->m_dev)) {
        rc = rtc_alarm_is_pending(this->m_dev, 0);
        if (rc < 0) {
            this->log_WARNING_HI_AlarmHardwareError(0, rc);
        }
    }

    // Boot seed: one polled read. If it fails or is implausible, the component starts
    // undisciplined and the first plausible update callback sample seeds it instead.
    std::int64_t rtc_s = 0;
    int read_rc = 0;
    if (this->readRtcSeconds(rtc_s, read_rc) == RtcRead::OK) {
        this->seedDiscipline(rtc_s);
    }

#if defined(CONFIG_RTC_UPDATE)
    if (!device_is_ready(this->m_dev)) {
        Fw::Logger::log("RTC not ready, update callback not armed. No corrections will be made.\n");
        return;
    }
    rc = rtc_update_set_callback(this->m_dev, RtcManager::static_update_callback_t, this);
    if (rc != 0) {
        Fw::Logger::log("RTC update callback not armed, rc = %d. No corrections will be made.\n", rc);
    }
#else
    Fw::Logger::log("RTC update callback not compiled in. No corrections will be made.\n");
#endif
}
// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void RtcManager ::timeGetPort_handler(FwIndexType portNum, Fw::Time& time) {
    // Get system uptime
    const std::int64_t uptime_us = RtcManager::uptimeUs();
    U32 seconds_since_boot = static_cast<U32>(uptime_us / 1000000);
    U32 useconds_since_boot = static_cast<U32>(uptime_us % 1000000);

    // Use proc time directly when the timebase parameter selects it
    Fw::ParamValid timeBaseValid;
    const Rtc::TimeBase timeBase = this->paramGet_TIMEBASE(timeBaseValid);
    if (timeBase == Rtc::TimeBase::TB_PROC_TIME) {
        time.set(::TimeBase::TB_PROC_TIME, 0, seconds_since_boot, useconds_since_boot);
        return;
    }

    // Read the disciplined time. Does not access the RTC hardware; must not emit events or
    // telemetry from this critical path.
    std::uint32_t seconds = 0;
    std::uint32_t useconds = 0;
    k_spinlock_key_t key = k_spin_lock(&this->m_lock);
    const bool seeded = this->m_discipline.read(uptime_us, seconds, useconds);
    k_spin_unlock(&this->m_lock, key);

    if (!seeded) {
        this->log_CONSOLE_RtcNotDisciplined();
        time.set(TimeBase::TB_PROC_TIME, 0, seconds_since_boot, useconds_since_boot);
        return;
    }
    this->log_CONSOLE_RtcNotDisciplined_ThrottleClear();

    time.set(TimeBase::TB_SC_TIME, 0, seconds, useconds);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void RtcManager ::TIME_SET_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Drv::TimeData& t) {
    // Check device readiness
    if (!device_is_ready(this->m_dev)) {
        // Emit device not ready event
        this->log_WARNING_HI_DeviceNotReady();

        // Send command response
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    this->log_WARNING_HI_DeviceNotReady_ThrottleClear();

    // Validate time data
    if (!this->timeDataIsValid(t)) {
        // Emit time not set event
        this->log_WARNING_HI_TimeNotSet(EINVAL);

        // Send command response
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    // Store current time for logging
    Fw::Time time_before_set = this->getTime();

    // Cancel any running sequences before setting time, as time change may impact their behavior
    for (FwIndexType i = 0; i < this->getNum_cancelSequences_OutputPorts(); i++) {
        if (!this->isConnected_cancelSequences_OutputPort(i)) {
            continue;
        }
        this->cancelSequences_out(i);
    }

    // Populate rtc_time structure from TimeData
    const struct rtc_time time_rtc = {
        .tm_sec = static_cast<int>(t.get_Second()),
        .tm_min = static_cast<int>(t.get_Minute()),
        .tm_hour = static_cast<int>(t.get_Hour()),
        .tm_mday = static_cast<int>(t.get_Day()),
        .tm_mon = static_cast<int>(t.get_Month() - 1),     // month [0-11]
        .tm_year = static_cast<int>(t.get_Year() - 1900),  // year since 1900
        .tm_wday = 0,
        .tm_yday = 0,
        .tm_isdst = 0,
    };

    // Set time on RTC
    const int rc = rtc_set_time(this->m_dev, &time_rtc);
    if (rc != 0) {
        // Emit time not set event
        this->log_WARNING_HI_TimeNotSet(rc);

        // Send command response
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // Seed the time offset from the new time. The RV3028 resets its sub-second divider when the
    // seconds are written, so this seed has no sub-second error. Reported time can step backward
    // one time.
    std::int64_t new_rtc_s = 0;
    if (RtcManager::rtcTimeToSeconds(time_rtc, new_rtc_s)) {
        this->seedDiscipline(new_rtc_s);
    }

    // Emit time set event, include previous time for reference
    this->log_ACTIVITY_HI_TimeSet(time_before_set.getSeconds(), time_before_set.getUSeconds());

    // Send command response
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void RtcManager ::parameterUpdated(FwPrmIdType id) {
    if (id != RtcManager::PARAMID_TIMEBASE) {
        return;
    }

    Fw::ParamValid valid;
    const Rtc::TimeBase timeBase = this->paramGet_TIMEBASE(valid);
    if ((valid == Fw::ParamValid::INVALID) || (valid == Fw::ParamValid::UNINIT)) {
        return;
    }

    // Cancel any running sequences, as the change in reported time may impact their behavior
    for (FwIndexType i = 0; i < this->getNum_cancelSequences_OutputPorts(); i++) {
        if (!this->isConnected_cancelSequences_OutputPort(i)) {
            continue;
        }
        this->cancelSequences_out(i);
    }

    this->log_ACTIVITY_HI_TimeBaseChanged(timeBase);
}

void RtcManager ::ALARM_SET_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Drv::TimeData& t) {
    // retrieve info about current alarm

    uint16_t mask = this->m_curr_mask;
    int rc = rtc_alarm_get_time(this->m_dev, 0, &mask, &this->m_alarm_time);

    // if alarm is already set, return an EALREADY error
    if (rc == 0 && mask != 0) {
        this->log_WARNING_HI_AlarmNotSet(t, EALREADY);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    // populate alarm time
    this->m_alarm_time.tm_sec = t.get_Second();
    this->m_alarm_time.tm_min = t.get_Minute();
    this->m_alarm_time.tm_hour = t.get_Hour();
    this->m_alarm_time.tm_mday = t.get_Day();
    this->m_alarm_time.tm_mon = t.get_Month() - 1;
    this->m_alarm_time.tm_year = t.get_Year() - 1900;

    // assure alarm is at a future point in time
    struct rtc_time c_time;
    struct rtc_time a_time = this->m_alarm_time;
    rc = rtc_get_time(this->m_dev, &c_time);
    if (rc != 0) {
        // indicates hardware error
        this->log_WARNING_HI_AlarmHardwareError(0, rc);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    int c_seconds = timeutil_timegm(rtc_time_to_tm(&c_time));
    int a_seconds = timeutil_timegm(rtc_time_to_tm(&a_time));
    if (a_seconds <= c_seconds) {
        this->log_WARNING_HI_AlarmNotSet(t, EINVAL);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // clear a stale alarm flag (AF) before arming, so registering the callback
    // below cannot trigger immediately. This also covers a failed clear in
    // configure().
    rc = rtc_alarm_is_pending(this->m_dev, 0);
    if (rc < 0) {
        this->log_WARNING_HI_AlarmHardwareError(0, rc);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // set the alarm
    rc = rtc_alarm_set_time(this->m_dev, 0, this->m_curr_mask, &this->m_alarm_time);

    // capture the return code for setting the alarm
    if (rc != 0) {
        // log failure
        this->log_WARNING_HI_AlarmHardwareError(0, rc);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // callback to trigger upon alarm trigger
    rc = rtc_alarm_set_callback(this->m_dev, 0, RtcManager::static_alarm_callback_t, this);

    // capture return code for the callback
    if (rc != 0) {
        // log failure
        this->log_WARNING_HI_AlarmHardwareError(0, rc);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // log success
    this->log_ACTIVITY_HI_AlarmSet(0, t);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void RtcManager ::ALARM_CANCEL_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U16 ID) {
    // check if it's present
    uint16_t mask = this->m_curr_mask;
    int rc = rtc_alarm_get_time(this->m_dev, 0, &mask, &this->m_alarm_time);

    if (rc != 0) {
        // indicates hardware error
        this->log_WARNING_HI_AlarmHardwareError(0, rc);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    if (mask != 0) {
        // disarm: unregister callback, write disabled alarm, clear stale AF
        rc = this->disarmAlarm();
        if (rc < 0) {
            // log failure
            this->log_WARNING_HI_AlarmHardwareError(0, rc);
            this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
            return;
        }

        this->log_ACTIVITY_HI_AlarmCanceled(ID);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        return;
    }

    // handle no alarm case
    this->log_WARNING_HI_AlarmNotCanceled(ID, 0);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
}

void RtcManager ::ALARM_LIST_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // check if present, if so log its info.
    uint16_t mask = this->m_curr_mask;
    int rc = rtc_alarm_get_time(this->m_dev, 0, &mask, &this->m_alarm_time);

    // if the return code is nonzero, log the error.
    if (rc != 0) {
        // indicates hardware error
        this->log_WARNING_HI_AlarmHardwareError(0, rc);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }

    // check the current mask to see if an alarm is active
    if (mask > 0) {
        // convert alarm time and log it
        Drv::TimeData alarm_time_value(this->m_alarm_time.tm_year + 1900, this->m_alarm_time.tm_mon + 1,
                                       this->m_alarm_time.tm_mday, this->m_alarm_time.tm_hour,
                                       this->m_alarm_time.tm_min, this->m_alarm_time.tm_sec);
        log_ACTIVITY_HI_AlarmSet(0, alarm_time_value);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        return;
    }

    // no alarm is present, log accordingly.
    Drv::TimeData alarm_none;
    this->log_WARNING_HI_AlarmNotSet(alarm_none, 0);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

void RtcManager::static_alarm_callback_t(const device* dev, uint16_t id, void* user_data) {
    // Reconstruct the object pointer from user_data
    RtcManager* instance = static_cast<RtcManager*>(user_data);
    if (instance != nullptr) {
        instance->alarm_callback_t(dev, id);
    }
}

void RtcManager ::alarm_callback_t(const struct device* dev, uint16_t id) {
    // Inform downstream components of the alarm trigger
    this->log_ACTIVITY_HI_AlarmTriggered(id);
    for (int i = 0; i < getNum_alarmTriggered_OutputPorts(); i++) {
        if (!this->isConnected_alarmTriggered_OutputPort(i)) {
            continue;
        }
        this->alarmTriggered_out(i);
    }

    // disarm the alarm, so it won't go off repeatedly.
    int rc = this->disarmAlarm();
    if (rc < 0) {
        // log failure
        this->log_WARNING_HI_AlarmHardwareError(0, rc);
    }
}

int RtcManager ::disarmAlarm() {
    // Unregister the callback first. This also disables the alarm interrupt
    // (AIE), so writing the disabled alarm below cannot raise a false
    // AlarmTriggered even though it may set AF on the RV3028.
    int rc = rtc_alarm_set_callback(this->m_dev, 0, nullptr, nullptr);
    if (rc < 0) {
        return rc;
    }

    // Write the disabled alarm (mask 0).
    uint16_t mask = 0;
    rc = rtc_alarm_set_time(this->m_dev, 0, mask, &this->m_alarm_time);
    if (rc < 0) {
        return rc;
    }

    // Clear a stale alarm flag (AF), now that the interrupt is disabled, so a
    // future alarm registration does not trigger immediately.
    return rtc_alarm_is_pending(this->m_dev, 0);
}

void RtcManager::static_update_callback_t(const struct device* dev, void* user_data) {
    // Reconstruct the object pointer from user_data
    RtcManager* instance = static_cast<RtcManager*>(user_data);
    if (instance != nullptr) {
        instance->update_callback_t();
    }
}

void RtcManager ::update_callback_t() {
    // Capture uptime as close to the RTC read as possible, before the (possibly slow) I2C
    // transaction, to minimize the callback-delay error in the correction.
    const std::int64_t uptime_us = RtcManager::uptimeUs();

    std::int64_t rtc_s = 0;
    int rc = 0;
    const RtcRead read = this->readRtcSeconds(rtc_s, rc);
    if (read != RtcRead::OK) {
        // RTC read failed or was implausible. Time offset does not change. Count it and warn
        // (throttled), so a stale TimeCorrectionUs is not mistaken for a healthy one.
        ++this->m_disciplineReadFaults;
        this->tlmWrite_DisciplineReadFaults(this->m_disciplineReadFaults);
        if (read == RtcRead::FAILED) {
            this->log_WARNING_LO_DisciplineReadFailed(rc);
        } else {
            this->log_WARNING_LO_DisciplineSampleImplausible(rtc_s);
        }
        return;
    }

    k_spinlock_key_t key = k_spin_lock(&this->m_lock);
    const TimeDiscipline::CorrectionResult result = this->m_discipline.correct(rtc_s, uptime_us);
    k_spin_unlock(&this->m_lock, key);

    // Emit telemetry and events only after releasing the spinlock; this runs on the system
    // workqueue thread, not the timeGetPort caller's thread, so it is safe to do so here.
    switch (result.kind) {
        case TimeDiscipline::Correction::APPLIED:
            this->tlmWrite_TimeCorrectionUs(result.correction_us);
            break;
        case TimeDiscipline::Correction::STEPPED:
            this->tlmWrite_TimeCorrectionUs(result.correction_us);
            this->log_WARNING_LO_TimeStepped(result.correction_us);
            break;
        case TimeDiscipline::Correction::REJECTED:
            ++this->m_disciplineRejects;
            this->tlmWrite_DisciplineRejects(this->m_disciplineRejects);
            break;
        case TimeDiscipline::Correction::IGNORED:
        default:
            // No telemetry, no event.
            break;
    }
}

RtcManager::RtcRead RtcManager ::readRtcSeconds(std::int64_t& rtc_s, int& rc) {
    if (!device_is_ready(this->m_dev)) {
        rc = -ENODEV;
        return RtcRead::FAILED;
    }

    struct rtc_time time_rtc = {};
    rc = rtc_get_time(this->m_dev, &time_rtc);
    if (rc != 0) {
        return RtcRead::FAILED;
    }

    return RtcManager::rtcTimeToSeconds(time_rtc, rtc_s) ? RtcRead::OK : RtcRead::IMPLAUSIBLE;
}

bool RtcManager ::rtcTimeToSeconds(const struct rtc_time& time_rtc, std::int64_t& rtc_s) {
    struct rtc_time time_rtc_mut = time_rtc;
    struct tm* time_tm = rtc_time_to_tm(&time_rtc_mut);
    errno = 0;
    const std::int64_t seconds = static_cast<std::int64_t>(timeutil_timegm(time_tm));
    if (errno == ERANGE) {
        rtc_s = -1;
        return false;
    }
    if (!TimeDiscipline::isPlausibleRtcSeconds(seconds)) {
        rtc_s = seconds;
        return false;
    }

    rtc_s = seconds;
    return true;
}

void RtcManager ::seedDiscipline(std::int64_t rtc_s) {
    const std::int64_t uptime_us = RtcManager::uptimeUs();
    k_spinlock_key_t key = k_spin_lock(&this->m_lock);
    this->m_discipline.seed(rtc_s, uptime_us);
    k_spin_unlock(&this->m_lock, key);
}

std::int64_t RtcManager ::uptimeUs() {
    return static_cast<std::int64_t>(k_ticks_to_us_floor64(k_uptime_ticks()));
}

void RtcManager ::log_CONSOLE_RtcNotDisciplined() {
    // Check throttle value
    if (this->m_RtcNotDisciplinedThrottle) {
        return;
    }

    // Set throttle
    this->m_RtcNotDisciplinedThrottle = true;

    // Emit the log message
    Fw::Logger::log("RTC not disciplined, falling back to uptime\n");
}

void RtcManager ::log_CONSOLE_RtcNotDisciplined_ThrottleClear() {
    // Reset throttle
    this->m_RtcNotDisciplinedThrottle = false;
}

bool RtcManager ::timeDataIsValid(Drv::TimeData t) {
    bool valid = true;

    if (t.get_Year() < 1900) {
        this->log_WARNING_HI_YearValidationFailed(t.get_Year());
        valid = false;
    }

    if (t.get_Month() < 1 || t.get_Month() > 12) {
        this->log_WARNING_HI_MonthValidationFailed(t.get_Month());
        valid = false;
    }

    if (t.get_Day() < 1 || t.get_Day() > 31) {
        this->log_WARNING_HI_DayValidationFailed(t.get_Day());
        valid = false;
    }

    if (t.get_Hour() > 23) {
        this->log_WARNING_HI_HourValidationFailed(t.get_Hour());
        valid = false;
    }

    if (t.get_Minute() > 59) {
        this->log_WARNING_HI_MinuteValidationFailed(t.get_Minute());
        valid = false;
    }

    if (t.get_Second() > 59) {
        this->log_WARNING_HI_SecondValidationFailed(t.get_Second());
        valid = false;
    }

    return valid;
}

}  // namespace Drv
