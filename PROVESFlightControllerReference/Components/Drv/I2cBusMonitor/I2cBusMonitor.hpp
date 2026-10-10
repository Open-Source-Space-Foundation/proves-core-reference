// ======================================================================
// \title  I2cBusMonitor.hpp
// \brief  hpp file for I2cBusMonitor component implementation class
// ======================================================================

#ifndef Drv_I2cBusMonitor_HPP
#define Drv_I2cBusMonitor_HPP

#include <Os/Mutex.hpp>

#include "PROVESFlightControllerReference/Components/Drv/I2cBusMonitor/I2cBusMonitorComponentAc.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/pinctrl.h>

namespace Drv {

//! Hardware needed to recover one DesignWare I2C bus
struct I2cBusMonitorConfig {
    const struct device* bus;                  //!< DesignWare I2C controller
    struct gpio_dt_spec scl;                   //!< SCL pin as a GPIO
    struct gpio_dt_spec sda;                   //!< SDA pin as a GPIO
    struct gpio_dt_spec muxReset;              //!< TCA954x reset line (port can be nullptr)
    const struct pinctrl_dev_config* pinctrl;  //!< Controller pinctrl
};

class I2cBusMonitor final : public I2cBusMonitorComponentBase {
  public:
    //! Construct I2cBusMonitor object
    I2cBusMonitor(const char* const compName);

    //! Destroy I2cBusMonitor object
    ~I2cBusMonitor();

    //! Store the bus hardware and register the recovery callback with the I2C driver
    void configure(const I2cBusMonitorConfig& config);

  private:
    //! Handler implementation for run
    void run_handler(FwIndexType portNum, U32 context) override;

    //! Handler implementation for command RECOVER_BUS
    void RECOVER_BUS_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    //! Recover the bus and report the result. The caller holds m_lock.
    int recover(bool force);

    //! Recovery callback. The I2C driver calls it with the bus lock held.
    static int recoverBusCallback(const struct device* dev);

    //! Do the recovery sequence
    int recoverLocked();

    //! Clock SCL until the target releases SDA, then send a STOP
    void clockOutBus();

    //! Read a line level: 1 = high, 0 = low or read error
    static U8 lineLevel(const struct gpio_dt_spec& spec);

    static I2cBusMonitor* s_instance;  //!< The instance for the C callback
    I2cBusMonitorConfig m_config{};    //!< Bus hardware
    Os::Mutex m_lock;                  //!< Serializes the run and command paths
    bool m_force = false;              //!< Recover when both lines are high
    bool m_acted = false;              //!< The callback did the recovery sequence
    U8 m_stuckSda = 1;                 //!< SDA level before recovery
    U8 m_stuckScl = 1;                 //!< SCL level before recovery
    U8 m_endSda = 1;                   //!< SDA level after recovery
    U8 m_endScl = 1;                   //!< SCL level after recovery
    U32 m_recoveryCount = 0;           //!< Recoveries that released the bus
    U32 m_recoveryFailureCount = 0;    //!< Recoveries that left a line low
    U32 m_cleanPolls = 0;              //!< Consecutive polls with both lines high
    U32 m_autoRecoveries = 0;          //!< Automatic recoveries since the last clean period
    int64_t m_lastAutoMs = 0;          //!< Uptime of the last automatic recovery, in ms
};

}  // namespace Drv

#endif
