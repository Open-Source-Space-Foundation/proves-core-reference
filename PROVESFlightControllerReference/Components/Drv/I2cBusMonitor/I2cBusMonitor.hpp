// ======================================================================
// \title  I2cBusMonitor.hpp
// \brief  hpp file for I2cBusMonitor component implementation class
// ======================================================================

#ifndef Drv_I2cBusMonitor_HPP
#define Drv_I2cBusMonitor_HPP

#include "PROVESFlightControllerReference/Components/Drv/I2cBusMonitor/I2cBusMonitorComponentAc.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/pinctrl.h>

namespace Drv {

//! Hardware needed to recover one DesignWare I2C bus
struct I2cBusMonitorConfig {
    const struct device* bus;                  //!< DesignWare I2C controller to monitor
    struct gpio_dt_spec scl;                   //!< SCL pin, used for sampling and bit-banging
    struct gpio_dt_spec sda;                   //!< SDA pin, used for sampling and bit-banging
    struct gpio_dt_spec muxReset;              //!< TCA954x reset line (port may be nullptr)
    const struct pinctrl_dev_config* pinctrl;  //!< Controller pinctrl, re-applied after bit-banging
};

class I2cBusMonitor final : public I2cBusMonitorComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct I2cBusMonitor object
    I2cBusMonitor(const char* const compName  //!< The component name
    );

    //! Destroy I2cBusMonitor object
    ~I2cBusMonitor();

  public:
    // ----------------------------------------------------------------------
    // Public helper methods
    // ----------------------------------------------------------------------

    //! Store the bus hardware and register the recovery callback with the I2C driver
    void configure(const I2cBusMonitorConfig& config);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    //!
    //! Sample SDA/SCL and recover the bus if a line is stuck low
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command RECOVER_BUS
    //!
    //! Force a bus recovery even if the lines read idle
    void RECOVER_BUS_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                U32 cmdSeq            //!< The command sequence number
                                ) override;

  private:
    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Run a recovery through i2c_recover_bus(), which holds the driver's bus lock
    //! \return 0 when both lines read high afterwards
    int recover(bool force,  //!< Recover even if the lines read idle
                bool& acted  //!< Set when the callback found a stuck bus (or was forced)
    );

    //! Emit events and telemetry for a recovery that acted
    void report(int rc);

    //! Recovery callback invoked by the DesignWare driver with its bus lock held
    static int recoverBusCallback(const struct device* dev);

    //! Recovery body; runs with the bus lock held, so no transfer is on the wire
    int recoverLocked();

    //! Clock SCL until the target releases SDA, then send a STOP
    int clockOutBus();

    //! Read a line level: 1 = high, 0 = low or read error
    static U8 lineLevel(const struct gpio_dt_spec& spec);

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    //! Bus hardware
    I2cBusMonitorConfig m_config;

    //! The single instance, for the C recovery callback
    static I2cBusMonitor* s_instance;

    //! Recover even if the lines read idle (set by recover() for the callback)
    bool m_force;

    //! Set by the callback when it found a stuck bus or was forced
    bool m_acted;

    //! SDA level seen by the callback before recovery
    U8 m_stuckSda;

    //! SCL level seen by the callback before recovery
    U8 m_stuckScl;

    //! Number of recoveries that released the bus
    U32 m_recoveryCount;

    //! Number of recoveries that left a line low
    U32 m_recoveryFailureCount;
};

}  // namespace Drv

#endif
