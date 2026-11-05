// ======================================================================
// \title  ReadinessMonitorTester.hpp
// \brief  hpp file for ReadinessMonitor component test harness implementation class
// ======================================================================

#ifndef Components_ReadinessMonitorTester_HPP
#define Components_ReadinessMonitorTester_HPP

#include <map>
#include <string>

#include "PROVESFlightControllerReference/Components/ReadinessMonitor/ReadinessMonitor.hpp"
#include "PROVESFlightControllerReference/Components/ReadinessMonitor/ReadinessMonitorGTestBase.hpp"

namespace Components {

class ReadinessMonitorTester final : public ReadinessMonitorGTestBase {
  public:
    static const FwSizeType MAX_HISTORY_SIZE = 64;
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;
    static const FwSizeType TEST_INSTANCE_QUEUE_DEPTH = 10;

    ReadinessMonitorTester();
    ~ReadinessMonitorTester();

    void testHealthyUnitPassesFullRun();
    void testFullRunLeavesUnbuiltChecksNotRun();
    void testWatchdogStallFails();
    void testStuckFileDownlinkWarnsThenClears();
    void testDeadFaceSensorFailsAndNamesTheSlot();
    void testUnpoweredFaceIsNotRead();
    void testAllSensorsDeadIsABusFault();
    void testDisarmedUnitFailsFlightConfig();
    void testMissingStartupSequenceFailsFlightConfig();
    void testGarbageRootEntryFailsFsRoot();
    void testMissingBootCountFailsFsRoot();
    void testRadioSilenceWarnsOnlyAfterQuiescence();
    void testBatteryMissingFailsAndLowWarns();
    void testLowFreeSpaceWarns();
    void testBootTriggersOneFullRun();
    void testPingIsAnswered();

  private:
    F64 from_tempGet_handler(FwIndexType portNum, Fw::Success& condition) override;
    F32 from_lightGet_handler(FwIndexType portNum, Fw::Success& condition) override;
    Fw::TlmValid from_tlmGet_handler(FwIndexType portNum,
                                     FwChanIdType id,
                                     Fw::Time& timeTag,
                                     Fw::TlmBuffer& val) override;
    Fw::ParamValid from_prmDbGet_handler(FwIndexType portNum, FwPrmIdType id, Fw::ParamBuffer& val) override;

    void connectPorts();
    void initComponents();

    template <typename T>
    void setChannel(FwChanIdType id, const T& value);
    void clearChannel(FwChanIdType id);

    template <typename T>
    void setParam(FwPrmIdType id, const T& value);

    void setFileQueueDepth(U32 depth);
    void powerFace(U8 face, bool on);

    //! Sends RUN_CHECKS and dispatches it
    void runChecks();

    //! One 1 Hz rate group call, dispatched
    void tick(U32 count = 1);

    void writeFile(const char* leaf, const char* contents);
    void removeFile(const char* leaf);

    U32 lastFailMask();
    U32 lastWarnMask();
    U32 lastPassMask();

    ReadinessMonitor component;
    ReadinessMonitor::Sources m_sources;
    std::string m_root;
    std::map<FwChanIdType, Fw::TlmBuffer> m_channels;
    std::map<FwPrmIdType, Fw::ParamBuffer> m_params;
    bool m_tempOk[ReadinessMonitor::NUM_TEMPGET_OUTPUT_PORTS];
    bool m_lightOk[ReadinessMonitor::NUM_LIGHTGET_OUTPUT_PORTS];
    //! Each tick advances WatchdogTransitions while set, like the 1 Hz pet
    bool m_petting = true;
    U32 m_transitions = 0;
};

}  // namespace Components

#endif
