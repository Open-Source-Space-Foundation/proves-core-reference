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
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

namespace Drv {

//! Hardware needed to recover one DesignWare I2C bus
struct I2cBusMonitorConfig {
    const struct device* bus;                  //!< DesignWare I2C controller
    struct gpio_dt_spec scl;                   //!< SCL pin as a GPIO
    struct gpio_dt_spec sda;                   //!< SDA pin as a GPIO
    struct gpio_dt_spec muxReset;              //!< TCA954x reset line (port can be nullptr)
    const struct pinctrl_dev_config* pinctrl;  //!< Controller pinctrl
    uintptr_t regs;                            //!< Controller register base, to abort and disable the controller
    const struct device* mux;                  //!< TCA954x root, to record the selected channel (can be nullptr)
};

//! Steps of the recovery sequence, as bits of SET_RECOVERY_STEPS
enum RecoveryStep : U8 {
    STEP_HALT = 1U << 0,          //!< Abort and disable the controller
    STEP_CLOCK_OUT = 1U << 1,     //!< Clock SCL while SDA is low, then send a STOP
    STEP_FORCE_CLOCKS = 1U << 2,  //!< Always clock SCL 9 times, then send a STOP
    STEP_MUX_RESET = 1U << 3,     //!< Pulse the TCA954x reset
};
constexpr U8 DEFAULT_RECOVERY_STEPS = STEP_HALT | STEP_CLOCK_OUT | STEP_MUX_RESET;

//! What started a recovery
enum class RecoverySource : U8 {
    DRIVER = 0,   //!< The I2C driver, after a timeout or on stuck bits in its state
    POLL = 1,     //!< The 1 Hz poll found a line low
    COMMAND = 2,  //!< RECOVER_BUS
};

//! Controller and line state recorded for one recovery
struct RecoveryDetail {
    U32 uptimeMs;
    RecoverySource source;
    U16 tar;          //!< IC_TAR: address of the target the controller last addressed
    U8 muxChannel;    //!< Channel mask the TCA954x driver last selected (0 = none)
    U32 status;       //!< IC_STATUS
    U32 rawIntr;      //!< IC_RAW_INTR_STAT
    U32 abortSource;  //!< IC_TX_ABRT_SOURCE
    U8 txflr;         //!< IC_TXFLR
    U8 rxflr;         //!< IC_RXFLR
    U8 lines;         //!< SDA (bit 0) and SCL (bit 1) before the sequence, then after each step, 2 bits each
    U8 clocks;        //!< SCL clocks sent
    U8 steps;         //!< RecoveryStep bits in use
    I32 ret;          //!< Result of the sequence
    U32 driver[8];    //!< DRIVER source only: reason and i2c_dw state (see i2c_dw_get_recovery_info)
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

    //! Handler implementation for command SET_RECOVERY_STEPS
    void SET_RECOVERY_STEPS_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U8 steps) override;

    //! Recover the bus and report the result. The caller holds m_lock.
    int recover(RecoverySource source);

    //! Emit the recovery details recorded since the last call
    void reportDetails();

    //! Report the recoveries that the driver ran after a transfer timeout
    void reportTimeoutRecoveries();

    //! Recovery callback. The I2C driver calls it with the bus lock held, either from recover() or
    //! from a transfer that timed out.
    static int recoverBusCallback(const struct device* dev);

    //! Do the recovery sequence
    int recoverLocked(RecoverySource source, const uint32_t* driverInfo);

    //! Record the controller registers and the selected mux channel
    void snapshotController(RecoveryDetail& d) const;

    //! Queue a detail for reportDetails(). The caller holds the driver bus_sem.
    void pushDetail(const RecoveryDetail& d);

    //! SDA and SCL levels, shifted into the 2-bit slot for a step of RecoveryDetail::lines
    U8 linesAt(U32 step) const;

    //! Clock SCL until the target releases SDA (or 9 times if forced), then send a STOP.
    //! Returns the clocks sent.
    U8 clockOutBus(bool force);

    //! Read a line level: 1 = high, 0 = low or read error
    static U8 lineLevel(const struct gpio_dt_spec& spec);

    //! Abort and disable the controller so that it lets go of the bus and flushes its FIFOs
    void haltController();

    static I2cBusMonitor* s_instance;                //!< The instance for the C callback
    I2cBusMonitorConfig m_config{};                  //!< Bus hardware
    Os::Mutex m_lock;                                //!< Serializes the run and command paths
    k_tid_t m_recoverThread = nullptr;               //!< Thread in recover(), to tell its callback from a driver one
    RecoverySource m_source = RecoverySource::POLL;  //!< What started recover()
    volatile U8 m_steps = DEFAULT_RECOVERY_STEPS;    //!< RecoveryStep bits to run
    bool m_acted = false;                            //!< The callback ran for recover()
    U8 m_stuckSda = 1;                               //!< SDA level before recovery
    U8 m_stuckScl = 1;                               //!< SCL level before recovery
    U8 m_endSda = 1;                                 //!< SDA level after recovery
    U8 m_endScl = 1;                                 //!< SCL level after recovery
    U32 m_recoveryCount = 0;                         //!< Recoveries that released the bus
    U32 m_recoveryFailureCount = 0;                  //!< Recoveries that left a line low
    U32 m_cleanPolls = 0;                            //!< Consecutive polls with both lines high
    atomic_t m_timeoutRecoveries = ATOMIC_INIT(0);   //!< Driver recoveries that released the bus
    atomic_t m_timeoutRecoveryFailures = ATOMIC_INIT(0);  //!< Driver recoveries that left a line low
    atomic_val_t m_reportedTimeoutRecoveries = 0;         //!< m_timeoutRecoveries at the last report
    atomic_t m_abortsCleared = ATOMIC_INIT(0);            //!< Driver calls for an ordinary abort flag, bus fine
    atomic_val_t m_reportedAbortsCleared = 0;             //!< m_abortsCleared at the last report
    atomic_val_t m_reportedTimeoutRecoveryFailures = 0;   //!< m_timeoutRecoveryFailures at the last report
    static constexpr U32 DETAIL_SLOTS = 16;
    RecoveryDetail m_details[DETAIL_SLOTS]{};    //!< Ring of recovery details, written under bus_sem
    atomic_t m_detailHead = ATOMIC_INIT(0);      //!< Next slot to write
    atomic_t m_detailTail = ATOMIC_INIT(0);      //!< Next slot to report
    atomic_t m_detailsDropped = ATOMIC_INIT(0);  //!< Details lost because the ring was full
    U32 m_autoRecoveries = 0;                    //!< Automatic recoveries since the last clean period
    int64_t m_lastAutoMs = 0;                    //!< Uptime of the last automatic recovery, in ms
};

}  // namespace Drv

#endif
