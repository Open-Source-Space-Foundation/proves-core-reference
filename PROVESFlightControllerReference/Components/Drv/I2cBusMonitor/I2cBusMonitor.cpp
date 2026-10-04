// ======================================================================
// \title  I2cBusMonitor.cpp
// \brief  cpp file for I2cBusMonitor component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/Drv/I2cBusMonitor/I2cBusMonitor.hpp"

#include <zephyr/kernel.h>

// Exported by drivers/i2c/i2c_dw.c; the header lives outside the public include path.
extern "C" void i2c_dw_register_recover_bus_cb(const struct device* dw_i2c_dev,
                                               i2c_api_recover_bus_t recover_bus_cb,
                                               const struct device* wrapper_dev);

namespace Drv {

namespace {
//! TCA954x reset pulse width (datasheet minimum is 6 ns)
constexpr U32 MUX_RESET_PULSE_US = 10;
//! Half of an SCL period while bit-banging (~50 kHz)
constexpr U32 HALF_CLOCK_US = 10;
//! Clocks needed to finish any byte plus its ACK
constexpr U32 RECOVERY_CLOCKS = 9;
}  // namespace

I2cBusMonitor* I2cBusMonitor::s_instance = nullptr;

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

I2cBusMonitor ::I2cBusMonitor(const char* const compName)
    : I2cBusMonitorComponentBase(compName),
      m_config(),
      m_force(false),
      m_acted(false),
      m_stuckSda(1),
      m_stuckScl(1),
      m_recoveryCount(0),
      m_recoveryFailureCount(0) {}

I2cBusMonitor ::~I2cBusMonitor() {}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

void I2cBusMonitor ::configure(const I2cBusMonitorConfig& config) {
    this->m_config = config;
    s_instance = this;
    i2c_dw_register_recover_bus_cb(config.bus, &I2cBusMonitor::recoverBusCallback, config.bus);
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void I2cBusMonitor ::run_handler(FwIndexType portNum, U32 context) {
    // Cheap unlocked check. A low line here may just be a transfer in flight;
    // the callback re-checks once it holds the bus lock.
    if (lineLevel(this->m_config.sda) && lineLevel(this->m_config.scl)) {
        return;
    }

    bool acted = false;
    int rc = this->recover(false, acted);
    if (acted) {
        this->report(rc);
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void I2cBusMonitor ::RECOVER_BUS_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    bool acted = false;
    int rc = this->recover(true, acted);
    this->report(rc);
    this->cmdResponse_out(opCode, cmdSeq, (rc == 0) ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

int I2cBusMonitor ::recover(bool force, bool& acted) {
    if (!device_is_ready(this->m_config.bus)) {
        acted = false;
        return -ENODEV;
    }
    this->m_force = force;
    this->m_acted = false;
    int rc = i2c_recover_bus(this->m_config.bus);
    acted = this->m_acted;
    return rc;
}

void I2cBusMonitor ::report(int rc) {
    if (this->m_stuckSda == 0 || this->m_stuckScl == 0) {
        this->log_WARNING_HI_BusStuck(this->m_stuckSda, this->m_stuckScl);
    }

    U8 sda = lineLevel(this->m_config.sda);
    U8 scl = lineLevel(this->m_config.scl);
    if (rc == 0) {
        this->m_recoveryCount++;
        this->tlmWrite_RecoveryCount(this->m_recoveryCount);
        this->log_ACTIVITY_HI_BusRecovered(sda, scl);
        this->log_WARNING_HI_BusStuck_ThrottleClear();
        this->log_WARNING_HI_BusRecoveryFailed_ThrottleClear();
    } else {
        this->m_recoveryFailureCount++;
        this->tlmWrite_RecoveryFailureCount(this->m_recoveryFailureCount);
        this->log_WARNING_HI_BusRecoveryFailed(rc, sda, scl);
    }
}

int I2cBusMonitor ::recoverBusCallback(const struct device* dev) {
    if (s_instance == nullptr) {
        return -ENODEV;
    }
    return s_instance->recoverLocked();
}

int I2cBusMonitor ::recoverLocked() {
    const I2cBusMonitorConfig& cfg = this->m_config;

    this->m_stuckSda = lineLevel(cfg.sda);
    this->m_stuckScl = lineLevel(cfg.scl);
    if (!this->m_force && this->m_stuckSda && this->m_stuckScl) {
        return 0;
    }
    this->m_acted = true;

    // A mux left mid-byte (or a device behind a still-selected channel) is cleared by the mux reset.
    // The TCA954x driver's cached channel may now be stale; its next transfer fails once and resyncs.
    if (cfg.muxReset.port != nullptr) {
        gpio_pin_set_dt(&cfg.muxReset, 1);
        k_busy_wait(MUX_RESET_PULSE_US);
        gpio_pin_set_dt(&cfg.muxReset, 0);
        k_busy_wait(MUX_RESET_PULSE_US);
    }

    // Targets on the trunk (INA219, MCP23017) have no reset line; clock them out.
    if (this->m_force || !lineLevel(cfg.sda) || !lineLevel(cfg.scl)) {
        int rc = this->clockOutBus();
        if (rc != 0) {
            return rc;
        }
    }

    return (lineLevel(cfg.sda) && lineLevel(cfg.scl)) ? 0 : -EBUSY;
}

int I2cBusMonitor ::clockOutBus() {
    const I2cBusMonitorConfig& cfg = this->m_config;
    const gpio_flags_t flags = GPIO_INPUT | GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN;

    int rc = gpio_pin_configure_dt(&cfg.scl, flags);
    if (rc == 0) {
        rc = gpio_pin_configure_dt(&cfg.sda, flags);
    }

    if (rc == 0) {
        for (U32 i = 0; i < RECOVERY_CLOCKS && !lineLevel(cfg.sda); i++) {
            gpio_pin_set_dt(&cfg.scl, 0);
            k_busy_wait(HALF_CLOCK_US);
            gpio_pin_set_dt(&cfg.scl, 1);
            k_busy_wait(HALF_CLOCK_US);
        }

        // STOP: SDA rises while SCL is high
        gpio_pin_set_dt(&cfg.scl, 0);
        k_busy_wait(HALF_CLOCK_US);
        gpio_pin_set_dt(&cfg.sda, 0);
        k_busy_wait(HALF_CLOCK_US);
        gpio_pin_set_dt(&cfg.scl, 1);
        k_busy_wait(HALF_CLOCK_US);
        gpio_pin_set_dt(&cfg.sda, 1);
        k_busy_wait(HALF_CLOCK_US);
    }

    // Hand the pins back to the I2C controller even if configuring them failed part way
    int pinctrlRc = pinctrl_apply_state(cfg.pinctrl, PINCTRL_STATE_DEFAULT);
    return (rc != 0) ? rc : pinctrlRc;
}

U8 I2cBusMonitor ::lineLevel(const struct gpio_dt_spec& spec) {
    return (gpio_pin_get_dt(&spec) > 0) ? 1 : 0;
}

}  // namespace Drv
