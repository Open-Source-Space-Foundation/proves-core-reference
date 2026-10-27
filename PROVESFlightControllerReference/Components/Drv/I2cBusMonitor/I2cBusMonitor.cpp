// ======================================================================
// \title  I2cBusMonitor.cpp
// \brief  cpp file for I2cBusMonitor component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/Drv/I2cBusMonitor/I2cBusMonitor.hpp"

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/sys_io.h>

// Defined in drivers/i2c/i2c_dw.c.
extern "C" void i2c_dw_register_recover_bus_cb(const struct device* dw_i2c_dev,
                                               i2c_api_recover_bus_t recover_bus_cb,
                                               const struct device* wrapper_dev);
// Defined in drivers/i2c/i2c_dw.c by patches/zephyr-i2c-dw-timeout-recover-bus.patch: why the driver called the
// callback, then IC_INTR_MASK, xfr_len, rx_pending, request_bytes, state, xfr_flags and messages left.
extern "C" void i2c_dw_get_recovery_info(uint32_t info[8]);
// Defined in drivers/i2c/i2c_tca954x.c by patches/zephyr-tca954x-channel-cache.patch.
extern "C" uint8_t tca954x_selected_channel(const struct device* root);
extern "C" void tca954x_forget_channel(const struct device* root);

namespace Drv {

namespace {
constexpr U32 HALF_CLOCK_US = 10;           //!< Half SCL period while bit-banging (~50 kHz)
constexpr U32 RECOVERY_CLOCKS = 9;          //!< Clocks to finish a byte and its ACK
constexpr U32 MUX_RESET_PULSE_US = 10;      //!< TCA954x reset pulse width
constexpr U32 CLEAN_POLLS_TO_CLEAR = 10;    //!< Clean polls that end back-off and clear the throttles
constexpr U32 BACKOFF_AFTER = 5;            //!< Automatic recoveries before back-off starts
constexpr int64_t BACKOFF_INTERVAL_S = 30;  //!< Minimum time between automatic recoveries in back-off
constexpr U32 DETAILS_PER_POLL = 8;         //!< Recovery details reported per 1 Hz poll
constexpr int64_t SHED_WINDOW_S = 60;       //!< Window over which timeouts count toward turning a face off

// DesignWare registers
constexpr uintptr_t IC_TAR_OFFSET = 0x04;
constexpr uintptr_t IC_RAW_INTR_STAT_OFFSET = 0x34;
constexpr uintptr_t IC_CLR_INTR_OFFSET = 0x40;
constexpr uintptr_t IC_ENABLE_OFFSET = 0x6c;
constexpr uintptr_t IC_STATUS_OFFSET = 0x70;
constexpr uintptr_t IC_TXFLR_OFFSET = 0x74;
constexpr uintptr_t IC_RXFLR_OFFSET = 0x78;
constexpr uintptr_t IC_TX_ABRT_SOURCE_OFFSET = 0x80;
constexpr uintptr_t IC_ENABLE_STATUS_OFFSET = 0x9c;
constexpr U32 IC_ENABLE_EN = 1U << 0;
constexpr U32 IC_ENABLE_ABORT = 1U << 1;
constexpr U32 IC_TAR_MASK = 0x3ff;

// i2c_dw driver values (drivers/i2c/i2c_dw.h and the timeout patch)
constexpr U32 DW_RECOVERY_TIMEOUT = 1;  //!< A transfer timed out
constexpr U32 DW_RECOVERY_STUCK = 2;    //!< A new transfer found stuck or abort bits in the driver state
constexpr U32 DW_SCL_STUCK = 1U << 6;
constexpr U32 DW_SDA_STUCK = 1U << 7;
constexpr U32 CONTROLLER_TIMEOUT_US = 10000;  //!< Limit for the abort and the disable to finish
constexpr U32 CONTROLLER_POLL_US = 10;
}  // namespace

I2cBusMonitor* I2cBusMonitor::s_instance = nullptr;

I2cBusMonitor::I2cBusMonitor(const char* const compName) : I2cBusMonitorComponentBase(compName) {}

I2cBusMonitor::~I2cBusMonitor() {}

void I2cBusMonitor::configure(const I2cBusMonitorConfig& config) {
    this->m_config = config;
    s_instance = this;
    i2c_dw_register_recover_bus_cb(config.bus, &I2cBusMonitor::recoverBusCallback, config.bus);
}

void I2cBusMonitor::run_handler(FwIndexType portNum, U32 context) {
    Os::ScopeLock lock(this->m_lock);

    this->reportTimeoutRecoveries();
    this->reportDetails();
    this->shedJammingFaces();

    if (lineLevel(this->m_config.sda) && lineLevel(this->m_config.scl)) {
        // Count clean polls. Enough of them end back-off and clear the throttles.
        if (this->m_cleanPolls < CLEAN_POLLS_TO_CLEAR && ++this->m_cleanPolls == CLEAN_POLLS_TO_CLEAR) {
            this->m_autoRecoveries = 0;
            this->log_WARNING_HI_BusStuck_ThrottleClear();
            this->log_ACTIVITY_HI_BusRecovered_ThrottleClear();
            this->log_WARNING_HI_BusRecoveryFailed_ThrottleClear();
            this->log_WARNING_HI_TimeoutRecoveries_ThrottleClear();
            this->log_WARNING_LO_RecoveryDetailsDropped_ThrottleClear();
        }
        return;
    }

    // In back-off, do not recover until the interval is over.
    const int64_t nowMs = k_uptime_get();
    if (this->m_autoRecoveries >= BACKOFF_AFTER && (nowMs - this->m_lastAutoMs) < BACKOFF_INTERVAL_S * 1000) {
        return;
    }

    (void)this->recover(RecoverySource::POLL);
    if (this->m_acted) {
        this->m_lastAutoMs = nowMs;
        if (++this->m_autoRecoveries == BACKOFF_AFTER) {
            this->log_WARNING_HI_AutoRecoveryBackOff(BACKOFF_AFTER, static_cast<U32>(BACKOFF_INTERVAL_S));
        }
    }
}

void I2cBusMonitor::RECOVER_BUS_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Os::ScopeLock lock(this->m_lock);
    const int rc = this->recover(RecoverySource::COMMAND);
    this->reportDetails();
    this->cmdResponse_out(opCode, cmdSeq, (rc == 0) ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

void I2cBusMonitor::SET_RECOVERY_STEPS_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U8 steps) {
    this->m_steps = steps;
    this->log_ACTIVITY_HI_RecoveryStepsSet(steps);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void I2cBusMonitor::SET_SHED_THRESHOLD_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U16 timeouts) {
    Os::ScopeLock lock(this->m_lock);
    this->m_shedThreshold = timeouts;
    this->log_ACTIVITY_HI_ShedThresholdSet(timeouts);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void I2cBusMonitor::shedJammingFaces() {
    const int64_t nowMs = k_uptime_get();
    const bool windowOver = (nowMs - this->m_shedWindowStartMs) >= SHED_WINDOW_S * 1000;
    for (U8 channel = 0; channel < MUX_CHANNELS; channel++) {
        atomic_t* const timeouts = &this->m_channelTimeouts[channel];
        // Power is the only thing that frees a face that grabs the bus again right after every recovery.
        // The load switch write goes over the same bus, so it relies on the driver recovering a timeout
        // and on the load switch retrying.
        const U32 count = static_cast<U32>(atomic_get(timeouts));
        if (this->m_shedThreshold != 0 && count >= this->m_shedThreshold &&
            this->isConnected_faceTurnOff_OutputPort(channel)) {
            this->log_WARNING_HI_FaceShed(channel, count);
            this->faceTurnOff_out(channel);
            atomic_clear(timeouts);
        } else if (windowOver) {
            atomic_clear(timeouts);
        }
    }
    if (windowOver) {
        this->m_shedWindowStartMs = nowMs;
    }
}

int I2cBusMonitor::recover(RecoverySource source) {
    this->m_source = source;
    this->m_acted = false;
    if (!device_is_ready(this->m_config.bus)) {
        return -ENODEV;
    }

    // Take the driver bus lock and call recoverBusCallback().
    this->m_recoverThread = k_current_get();
    const int rc = i2c_recover_bus(this->m_config.bus);
    this->m_recoverThread = nullptr;
    if (!this->m_acted) {
        return rc;
    }

    this->m_cleanPolls = 0;
    if (this->m_stuckSda == 0 || this->m_stuckScl == 0) {
        this->log_WARNING_HI_BusStuck(this->m_stuckSda, this->m_stuckScl);
    }
    if (rc == 0) {
        this->tlmWrite_RecoveryCount(++this->m_recoveryCount);
        this->log_ACTIVITY_HI_BusRecovered();
    } else {
        this->tlmWrite_RecoveryFailureCount(++this->m_recoveryFailureCount);
        this->log_WARNING_HI_BusRecoveryFailed(rc, this->m_endSda, this->m_endScl);
    }
    return rc;
}

void I2cBusMonitor::reportDetails() {
    const atomic_val_t dropped = atomic_clear(&this->m_detailsDropped);
    if (dropped != 0) {
        this->log_WARNING_LO_RecoveryDetailsDropped(static_cast<U32>(dropped));
    }
    for (U32 n = 0; n < DETAILS_PER_POLL; n++) {
        const atomic_val_t tail = atomic_get(&this->m_detailTail);
        if (tail == atomic_get(&this->m_detailHead)) {
            return;
        }
        const RecoveryDetail& d = this->m_details[static_cast<U32>(tail) % DETAIL_SLOTS];
        this->log_WARNING_LO_RecoveryDetail(d.uptimeMs, static_cast<U8>(d.source), d.tar, d.muxChannel, d.status,
                                            d.rawIntr, d.abortSource, d.txflr, d.rxflr, d.lines, d.clocks, d.steps,
                                            d.ret);
        if (d.source == RecoverySource::DRIVER) {
            this->log_WARNING_LO_RecoveryDriverState(d.uptimeMs, static_cast<U8>(d.driver[0]), d.driver[1], d.driver[2],
                                                     d.driver[3], d.driver[4], static_cast<U8>(d.driver[5]),
                                                     static_cast<U8>(d.driver[6]), static_cast<U8>(d.driver[7]));
        }
        atomic_set(&this->m_detailTail, tail + 1);
    }
}

void I2cBusMonitor::reportTimeoutRecoveries() {
    const atomic_val_t cleared = atomic_get(&this->m_abortsCleared);
    if (cleared != this->m_reportedAbortsCleared) {
        this->m_reportedAbortsCleared = cleared;
        this->tlmWrite_AbortFlagsCleared(static_cast<U32>(cleared));
    }

    const atomic_val_t recoveries = atomic_get(&this->m_timeoutRecoveries);
    const atomic_val_t failures = atomic_get(&this->m_timeoutRecoveryFailures);
    const U32 newRecoveries = static_cast<U32>(recoveries - this->m_reportedTimeoutRecoveries);
    const U32 newFailures = static_cast<U32>(failures - this->m_reportedTimeoutRecoveryFailures);
    if (newRecoveries == 0 && newFailures == 0) {
        return;
    }
    this->m_reportedTimeoutRecoveries = recoveries;
    this->m_reportedTimeoutRecoveryFailures = failures;
    this->m_cleanPolls = 0;
    this->m_recoveryCount += newRecoveries;
    this->m_recoveryFailureCount += newFailures;
    this->tlmWrite_RecoveryCount(this->m_recoveryCount);
    this->tlmWrite_RecoveryFailureCount(this->m_recoveryFailureCount);
    this->log_WARNING_HI_TimeoutRecoveries(newRecoveries, newFailures);
}

int I2cBusMonitor::recoverBusCallback(const struct device* dev) {
    I2cBusMonitor* const self = s_instance;
    if (self == nullptr) {
        return -ENODEV;
    }
    if (k_current_get() == self->m_recoverThread) {
        self->m_acted = true;
        return self->recoverLocked(self->m_source, nullptr);
    }

    // The driver called from a transfer on another thread. That thread holds bus_sem but not m_lock, so
    // count the result here and let the next run_handler() report it.
    uint32_t info[8];
    i2c_dw_get_recovery_info(info);
    const I2cBusMonitorConfig& cfg = self->m_config;
    if (info[0] == DW_RECOVERY_STUCK && (info[5] & (DW_SCL_STUCK | DW_SDA_STUCK)) == 0 && lineLevel(cfg.sda) &&
        lineLevel(cfg.scl)) {
        // Every abort that ends with a STOP, an address NACK included, leaves I2C_DW_USER_ABRT in the
        // driver state. The bus is fine, so only let the driver clear the flag.
        atomic_inc(&self->m_abortsCleared);
        return 0;
    }
    // Blame a timeout on the face whose channel was selected. The recovery's mux reset clears the selection.
    // A transfer that only finds stuck bits may be on a different channel from the one that left them.
    const U8 channel = (cfg.mux != nullptr) ? tca954x_selected_channel(cfg.mux) : 0;
    if (info[0] == DW_RECOVERY_TIMEOUT && channel != 0 && (channel & (channel - 1)) == 0) {
        atomic_inc(&self->m_channelTimeouts[__builtin_ctz(channel)]);
    }
    const int rc = self->recoverLocked(RecoverySource::DRIVER, info);
    atomic_inc((rc == 0) ? &self->m_timeoutRecoveries : &self->m_timeoutRecoveryFailures);
    return rc;
}

int I2cBusMonitor::recoverLocked(RecoverySource source, const uint32_t* driverInfo) {
    const I2cBusMonitorConfig& cfg = this->m_config;
    const U8 steps = this->m_steps;
    RecoveryDetail d{};
    d.uptimeMs = k_uptime_get_32();
    d.source = source;
    d.steps = steps;
    this->snapshotController(d);
    if (driverInfo != nullptr) {
        memcpy(d.driver, driverInfo, sizeof(d.driver));
    }

    this->m_stuckSda = lineLevel(cfg.sda);
    this->m_stuckScl = lineLevel(cfg.scl);
    d.lines = linesAt(0);

    // Stop the controller first. A controller that holds SCL low for an unread byte takes the
    // clock back as soon as the pins return to it.
    if (steps & STEP_HALT) {
        this->haltController();
    }
    d.lines |= linesAt(1);

    // Set SDA and SCL as open-drain GPIOs.
    const bool clock = (steps & (STEP_CLOCK_OUT | STEP_FORCE_CLOCKS)) != 0;
    const bool force = (steps & STEP_FORCE_CLOCKS) != 0;
    const gpio_flags_t flags = GPIO_INPUT | GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN;
    int rc = 0;
    if (clock) {
        rc = gpio_pin_configure_dt(&cfg.scl, flags);
        if (rc == 0) {
            rc = gpio_pin_configure_dt(&cfg.sda, flags);
        }
        if (rc == 0) {
            d.clocks = this->clockOutBus(force);
        }
    }
    d.lines |= linesAt(2);

    // Pulse the mux reset, then clock out again if a line is low.
    if ((steps & STEP_MUX_RESET) && cfg.muxReset.port != nullptr) {
        gpio_pin_set_dt(&cfg.muxReset, 1);
        k_busy_wait(MUX_RESET_PULSE_US);
        gpio_pin_set_dt(&cfg.muxReset, 0);
        k_busy_wait(MUX_RESET_PULSE_US);
        // The reset deselected every channel. Make the tca954x driver select its channel again.
        if (cfg.mux != nullptr) {
            tca954x_forget_channel(cfg.mux);
        }
    }
    d.lines |= linesAt(3);
    if (clock && rc == 0 && (!lineLevel(cfg.sda) || !lineLevel(cfg.scl))) {
        d.clocks += this->clockOutBus(force);
    }

    // Give the pins back to the I2C controller, restart it and read the final levels.
    const int pinctrlRc = pinctrl_apply_state(cfg.pinctrl, PINCTRL_STATE_DEFAULT);
    if (cfg.regs != 0) {
        sys_write32(IC_ENABLE_EN, cfg.regs + IC_ENABLE_OFFSET);
    }
    this->m_endSda = lineLevel(cfg.sda);
    this->m_endScl = lineLevel(cfg.scl);
    if (rc == 0) {
        rc = pinctrlRc;
    }
    if (rc == 0 && (this->m_endSda == 0 || this->m_endScl == 0)) {
        rc = -EBUSY;
    }
    d.ret = rc;
    this->pushDetail(d);
    return rc;
}

void I2cBusMonitor::snapshotController(RecoveryDetail& d) const {
    const uintptr_t regs = this->m_config.regs;
    if (regs != 0) {
        d.tar = static_cast<U16>(sys_read32(regs + IC_TAR_OFFSET) & IC_TAR_MASK);
        d.status = sys_read32(regs + IC_STATUS_OFFSET);
        d.rawIntr = sys_read32(regs + IC_RAW_INTR_STAT_OFFSET);
        d.abortSource = sys_read32(regs + IC_TX_ABRT_SOURCE_OFFSET);
        d.txflr = static_cast<U8>(sys_read32(regs + IC_TXFLR_OFFSET));
        d.rxflr = static_cast<U8>(sys_read32(regs + IC_RXFLR_OFFSET));
    }
    if (this->m_config.mux != nullptr) {
        d.muxChannel = tca954x_selected_channel(this->m_config.mux);
    }
}

void I2cBusMonitor::pushDetail(const RecoveryDetail& d) {
    // One producer at a time: every caller holds the driver bus_sem.
    const atomic_val_t head = atomic_get(&this->m_detailHead);
    if (head - atomic_get(&this->m_detailTail) >= static_cast<atomic_val_t>(DETAIL_SLOTS)) {
        atomic_inc(&this->m_detailsDropped);
        return;
    }
    this->m_details[static_cast<U32>(head) % DETAIL_SLOTS] = d;
    atomic_set(&this->m_detailHead, head + 1);
}

U8 I2cBusMonitor::linesAt(U32 step) const {
    const U8 lines = static_cast<U8>(lineLevel(this->m_config.sda) | (lineLevel(this->m_config.scl) << 1));
    return static_cast<U8>(lines << (2 * step));
}

U8 I2cBusMonitor::clockOutBus(bool force) {
    const I2cBusMonitorConfig& cfg = this->m_config;

    U8 clocks = 0;
    for (; clocks < RECOVERY_CLOCKS && (force || !lineLevel(cfg.sda)); clocks++) {
        gpio_pin_set_dt(&cfg.scl, 0);
        k_busy_wait(HALF_CLOCK_US);
        gpio_pin_set_dt(&cfg.scl, 1);
        k_busy_wait(HALF_CLOCK_US);
    }

    // Send a STOP: SDA goes high while SCL is high.
    gpio_pin_set_dt(&cfg.scl, 0);
    k_busy_wait(HALF_CLOCK_US);
    gpio_pin_set_dt(&cfg.sda, 0);
    k_busy_wait(HALF_CLOCK_US);
    gpio_pin_set_dt(&cfg.scl, 1);
    k_busy_wait(HALF_CLOCK_US);
    gpio_pin_set_dt(&cfg.sda, 1);
    k_busy_wait(HALF_CLOCK_US);
    return clocks;
}

void I2cBusMonitor::haltController() {
    const uintptr_t regs = this->m_config.regs;
    if (regs == 0) {
        return;
    }

    // ABORT makes the controller issue a STOP and release SCL. The hardware clears the bit when done.
    sys_write32(IC_ENABLE_EN | IC_ENABLE_ABORT, regs + IC_ENABLE_OFFSET);
    for (U32 t = 0; t < CONTROLLER_TIMEOUT_US && (sys_read32(regs + IC_ENABLE_OFFSET) & IC_ENABLE_ABORT);
         t += CONTROLLER_POLL_US) {
        k_busy_wait(CONTROLLER_POLL_US);
    }

    // Disabling flushes both FIFOs. It only completes once the controller has let go of the bus.
    sys_write32(0, regs + IC_ENABLE_OFFSET);
    for (U32 t = 0; t < CONTROLLER_TIMEOUT_US && (sys_read32(regs + IC_ENABLE_STATUS_OFFSET) & IC_ENABLE_EN);
         t += CONTROLLER_POLL_US) {
        k_busy_wait(CONTROLLER_POLL_US);
    }
    (void)sys_read32(regs + IC_CLR_INTR_OFFSET);
}

U8 I2cBusMonitor::lineLevel(const struct gpio_dt_spec& spec) {
    return (gpio_pin_get_dt(&spec) > 0) ? 1 : 0;
}

}  // namespace Drv
