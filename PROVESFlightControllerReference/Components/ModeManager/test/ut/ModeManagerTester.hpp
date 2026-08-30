// ======================================================================
// \title  ModeManagerTester.hpp
// \author ncc-michael
// \brief  hpp file for ModeManager component test harness implementation class
// ======================================================================

#ifndef Components_ModeManagerTester_HPP
#define Components_ModeManagerTester_HPP

#include "PROVESFlightControllerReference/Components/ModeManager/ModeManager.hpp"
#include "PROVESFlightControllerReference/Components/ModeManager/ModeManagerGTestBase.hpp"

namespace Components {

class ModeManagerTester final : public ModeManagerGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs.
    // Sized for the 10-second default debounce tests (20+ rate group ticks,
    // each writing three telemetry channels); History asserts on overflow.
    static const FwSizeType MAX_HISTORY_SIZE = 100;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    // Queue depth supplied to the component instance under test
    static const FwSizeType TEST_INSTANCE_QUEUE_DEPTH = 10;

    // Per-test state file path (relative, so it lands in the current working
    // directory) overriding the flight default /mode_state.bin
    static constexpr const char* TEST_STATE_FILE = "ModeManagerTester_state.bin";

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object ModeManagerTester
    //!
    //! deferBoot=false (default): parameters are loaded immediately, as the
    //! non-persistence tests expect. deferBoot=true: the test seeds the state
    //! file itself and then calls bootFromPersistentState(), which mirrors the
    //! topology boot ordering (restorePersistentState() before
    //! loadParameters(), see ReferenceDeploymentTopology.cpp).
    explicit ModeManagerTester(bool deferBoot = false);

    //! Destroy object ModeManagerTester
    ~ModeManagerTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! Component boots in NORMAL mode with reason NONE and entry count 0
    void testInitialState();

    //! FORCE_SAFE_MODE from NORMAL enters safe mode with reason GROUND_COMMAND
    void testForceSafeModeCommand();

    //! FORCE_SAFE_MODE while already in safe mode is an idempotent OK no-op
    void testForceSafeModeIdempotent();

    //! EXIT_SAFE_MODE restores NORMAL mode and clears the reason
    void testExitSafeModeCommand();

    //! EXIT_SAFE_MODE while already NORMAL still runs the full exit path
    void testExitSafeModeWhenAlreadyNormal();

    //! GET_CURRENT_MODE / GET_SAFE_MODE_REASON report safe mode state
    void testModeQueryCommandsInSafeMode();

    //! forceSafeMode port with reason NONE defaults to EXTERNAL_REQUEST
    void testForceSafeModePortDefaultsReason();

    //! forceSafeMode port passes an explicit reason (LORA) through
    void testForceSafeModePortExplicitReason();

    //! forceSafeMode port request is ignored while already in safe mode
    void testForceSafeModePortIgnoredInSafeMode();

    //! Entry threshold is strict: 6.7 V exactly does not trigger, below does
    void testVoltageEntryThresholdBoundary();

    //! Default debounce: 9 low-voltage seconds do nothing, the 10th enters
    void testVoltageEntryDebounceDefaultTenSeconds();

    //! One healthy reading resets the low-voltage debounce counter
    void testVoltageDebounceResetOnRecovery();

    //! Recovery threshold is strict: 8.0 V exactly does not recover, above does
    void testVoltageRecoveryThresholdBoundary();

    //! Recovery debounce counts consecutive healthy seconds and resets on dips
    void testVoltageRecoveryDebounce();

    //! No voltage auto-recovery when the reason is not LOW_BATTERY
    void testNoAutoRecoveryForNonLowBatteryReasons();

    //! Sequence completion with OK logs success and forwards the response
    void testSequenceCompletionOk();

    //! Sequence completion with an error logs failure and forwards the response
    void testSequenceCompletionFailure();

    //! Command loss timeout enters safe mode and stops the watchdog once
    void testCommandLossTriggersSafeModeAndWatchdogStop();

    //! packetRouted resets the command loss timer and debounce latch
    void testPacketRoutedResetsCommandLossTimer();

    //! prepareForReboot logs the event, persists cleanShutdown=1 without
    //! changing mode, and the next boot does not flag an unintended reboot
    void testPrepareForReboot();

    //! A state file with cleanShutdown=0 in NORMAL mode is an unintended
    //! reboot: the component boots into SYSTEM_FAULT safe mode
    void testRestoreUnintendedReboot();

    //! A clean-shutdown state file restores NORMAL quietly and re-arms the
    //! unclean flag for next-boot crash detection
    void testRestoreCleanShutdown();

    //! No state file (first boot): NORMAL, no warning, fresh unclean file
    void testRestoreNoStateFile();

    //! A persisted SAFE_MODE state is restored: switches off, reason and
    //! entry count preserved, no fresh safe-mode entry side effects
    void testRestoreSafeModeState();

    //! A truncated state file falls back to defaults with a load-read warning
    void testRestoreShortStateFile();

    //! An invalid persisted mode value falls back to defaults with a
    //! load-corrupt warning
    void testRestoreCorruptModeValue();

  private:
    // ----------------------------------------------------------------------
    // Handler overrides for typed from ports
    // ----------------------------------------------------------------------

    //! Override for from_voltageGet: records history and returns m_voltage
    F64 from_voltageGet_handler(FwIndexType portNum  //!< The port number
                                ) override;

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

    //! Invoke the 1Hz rate group port count times
    void tick(U32 count);

    //! Set the voltage debounce parameter and reload component parameters
    void setDebounceSeconds(U32 seconds);

    //! Boot the component the way the topology does: restorePersistentState()
    //! first, then loadParameters() (ReferenceDeploymentTopology.cpp ordering).
    //! Only valid after construction with deferBoot=true.
    void bootFromPersistentState();

    //! Seed the state file with a PersistentState record
    void seedStateFile(U8 mode, U32 safeModeEntryCount, U8 safeModeReason, U8 cleanShutdown);

    //! Seed the state file with raw bytes (for truncation/corruption tests)
    void seedRawStateFile(const U8* data, FwSizeType size);

    //! Read the state file back into a PersistentState record; returns
    //! whether a full record could be read
    bool readStateFile(ModeManager::PersistentState& state);

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    ModeManager component;

    //! Voltage returned to the component from the from_voltageGet handler
    F64 m_voltage = 8.5;
};

}  // namespace Components

#endif
