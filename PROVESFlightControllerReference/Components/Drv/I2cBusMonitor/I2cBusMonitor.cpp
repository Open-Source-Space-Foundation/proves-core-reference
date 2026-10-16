// ======================================================================
// \title  I2cBusMonitor.cpp
// \brief  cpp file for I2cBusMonitor component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/Drv/I2cBusMonitor/I2cBusMonitor.hpp"

#include <zephyr/kernel.h>

// Defined in drivers/i2c/i2c_dw.c.
extern "C" void i2c_dw_register_recover_bus_cb(const struct device* dw_i2c_dev,
                                               i2c_api_recover_bus_t recover_bus_cb,
                                               const struct device* wrapper_dev);

namespace Drv {

namespace {
constexpr U32 HALF_CLOCK_US = 10;           //!< Half SCL period while bit-banging (~50 kHz)
constexpr U32 RECOVERY_CLOCKS = 9;          //!< Clocks to finish a byte and its ACK
constexpr U32 MUX_RESET_PULSE_US = 10;      //!< TCA954x reset pulse width
constexpr U32 CLEAN_POLLS_TO_CLEAR = 10;    //!< Clean polls that end back-off and clear the throttles
constexpr U32 BACKOFF_AFTER = 5;            //!< Automatic recoveries before back-off starts
constexpr int64_t BACKOFF_INTERVAL_S = 30;  //!< Minimum time between automatic recoveries in back-off
}  // namespace

I2cBusMonitor* I2cBusMonitor::s_instance = nullptr;

I2cBusMonitor ::I2cBusMonitor(const char* const compName) : I2cBusMonitorComponentBase(compName) {}

I2cBusMonitor ::~I2cBusMonitor() {}

void I2cBusMonitor ::configure(const I2cBusMonitorConfig& config) {
    this->m_config = config;
    s_instance = this;
    i2c_dw_register_recover_bus_cb(config.bus, &I2cBusMonitor::recoverBusCallback, config.bus);
}

void I2cBusMonitor ::run_handler(FwIndexType portNum, U32 context) {
    Os::ScopeLock lock(this->m_lock);

    if (lineLevel(this->m_config.sda) && lineLevel(this->m_config.scl)) {
        // Count clean polls. Enough of them end back-off and clear the throttles.
        if (this->m_cleanPolls < CLEAN_POLLS_TO_CLEAR && ++this->m_cleanPolls == CLEAN_POLLS_TO_CLEAR) {
            this->m_autoRecoveries = 0;
            this->log_WARNING_HI_BusStuck_ThrottleClear();
            this->log_ACTIVITY_HI_BusRecovered_ThrottleClear();
            this->log_WARNING_HI_BusRecoveryFailed_ThrottleClear();
        }
        return;
    }

    // In back-off, do not recover until the interval is over.
    const int64_t nowMs = k_uptime_get();
    if (this->m_autoRecoveries >= BACKOFF_AFTER && (nowMs - this->m_lastAutoMs) < BACKOFF_INTERVAL_S * 1000) {
        return;
    }

    (void)this->recover(false);
    if (this->m_acted) {
        this->m_lastAutoMs = nowMs;
        if (++this->m_autoRecoveries == BACKOFF_AFTER) {
            this->log_WARNING_HI_AutoRecoveryBackOff(BACKOFF_AFTER, static_cast<U32>(BACKOFF_INTERVAL_S));
        }
    }
}

void I2cBusMonitor ::RECOVER_BUS_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Os::ScopeLock lock(this->m_lock);
    const int rc = this->recover(true);
    this->cmdResponse_out(opCode, cmdSeq, (rc == 0) ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

int I2cBusMonitor ::recover(bool force) {
    this->m_force = force;
    this->m_acted = false;
    if (!device_is_ready(this->m_config.bus)) {
        return -ENODEV;
    }

    // Take the driver bus lock and call recoverBusCallback().
    const int rc = i2c_recover_bus(this->m_config.bus);
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

int I2cBusMonitor ::recoverBusCallback(const struct device* dev) {
    return (s_instance == nullptr) ? -ENODEV : s_instance->recoverLocked();
}

int I2cBusMonitor ::recoverLocked() {
    const I2cBusMonitorConfig& cfg = this->m_config;

    this->m_stuckSda = lineLevel(cfg.sda);
    this->m_stuckScl = lineLevel(cfg.scl);
    if (!this->m_force && this->m_stuckSda && this->m_stuckScl) {
        return 0;
    }
    this->m_acted = true;

    // Set SDA and SCL as open-drain GPIOs.
    const gpio_flags_t flags = GPIO_INPUT | GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN;
    int rc = gpio_pin_configure_dt(&cfg.scl, flags);
    if (rc == 0) {
        rc = gpio_pin_configure_dt(&cfg.sda, flags);
    }

    // Clock out the bus, pulse the mux reset, then clock out again if a line is low.
    if (rc == 0) {
        this->clockOutBus();
        if (cfg.muxReset.port != nullptr) {
            gpio_pin_set_dt(&cfg.muxReset, 1);
            k_busy_wait(MUX_RESET_PULSE_US);
            gpio_pin_set_dt(&cfg.muxReset, 0);
            k_busy_wait(MUX_RESET_PULSE_US);
        }
        if (!lineLevel(cfg.sda) || !lineLevel(cfg.scl)) {
            this->clockOutBus();
        }
    }

    // Give the pins back to the I2C controller and read the final levels.
    const int pinctrlRc = pinctrl_apply_state(cfg.pinctrl, PINCTRL_STATE_DEFAULT);
    this->m_endSda = lineLevel(cfg.sda);
    this->m_endScl = lineLevel(cfg.scl);
    if (rc == 0) {
        rc = pinctrlRc;
    }
    if (rc == 0 && (this->m_endSda == 0 || this->m_endScl == 0)) {
        rc = -EBUSY;
    }
    return rc;
}

void I2cBusMonitor ::clockOutBus() {
    const I2cBusMonitorConfig& cfg = this->m_config;

    for (U32 i = 0; i < RECOVERY_CLOCKS && !lineLevel(cfg.sda); i++) {
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
}

U8 I2cBusMonitor ::lineLevel(const struct gpio_dt_spec& spec) {
    return (gpio_pin_get_dt(&spec) > 0) ? 1 : 0;
}

}  // namespace Drv
