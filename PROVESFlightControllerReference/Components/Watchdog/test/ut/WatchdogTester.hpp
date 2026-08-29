// ======================================================================
// \title  WatchdogTester.hpp
// \author ncc-michael
// \brief  hpp file for Watchdog component test harness implementation class
// ======================================================================

#ifndef Components_WatchdogTester_HPP
#define Components_WatchdogTester_HPP

#include "Drv/Ports/GpioStatusEnumAc.hpp"
#include "PROVESFlightControllerReference/Components/Watchdog/Watchdog.hpp"
#include "PROVESFlightControllerReference/Components/Watchdog/WatchdogGTestBase.hpp"

namespace Components {

class WatchdogTester final : public WatchdogGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 10;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object WatchdogTester
    WatchdogTester();

    //! Destroy object WatchdogTester
    ~WatchdogTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! Rate group calls toggle the GPIO and count transitions while started
    void testRunTogglesGpioWhileStarted();

    //! START_WATCHDOG responds OK, emits WatchdogStart, and enables petting
    void testStartCommand();

    //! STOP_WATCHDOG responds OK, signals prepareForReboot, and disables petting
    void testStopCommand();

    //! start/stop signal ports gate petting without touching prepareForReboot
    void testStartStopSignalPorts();

    //! Transition count and GPIO state persist across a stop/start cycle
    void testTransitionCountAcrossStopStart();

    //! A failing GPIO driver status is tolerated: no crash, petting continues
    void testGpioFailureIsTolerated();

  private:
    // ----------------------------------------------------------------------
    // Handler overrides for typed from ports
    // ----------------------------------------------------------------------

    //! Override for from_gpioSet: records history and returns m_gpioStatus
    Drv::GpioStatus from_gpioSet_handler(FwIndexType portNum,  //!< The port number
                                         const Fw::Logic& state) override;

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    Watchdog component;

    //! Status returned to the component from the from_gpioSet handler
    Drv::GpioStatus m_gpioStatus = Drv::GpioStatus::OP_OK;
};

}  // namespace Components

#endif
