// ======================================================================
// \title  ReadinessMonitorTestMain.cpp
// \brief  cpp file for ReadinessMonitor component test main function
// ======================================================================

#include "ReadinessMonitorTester.hpp"

#define READINESS_TEST(name)                  \
    TEST(ReadinessMonitor, name) {            \
        Components::ReadinessMonitorTester t; \
        t.test##name();                       \
    }

READINESS_TEST(HealthyUnitPassesFullRun)
READINESS_TEST(FullRunLeavesUnbuiltChecksNotRun)
READINESS_TEST(WatchdogStallFails)
READINESS_TEST(StuckFileDownlinkWarnsThenClears)
READINESS_TEST(DeadFaceSensorFailsAndNamesTheSlot)
READINESS_TEST(UnpoweredFaceIsNotRead)
READINESS_TEST(AllSensorsDeadIsABusFault)
READINESS_TEST(DisarmedUnitFailsFlightConfig)
READINESS_TEST(MissingStartupSequenceFailsFlightConfig)
READINESS_TEST(GarbageRootEntryFailsFsRoot)
READINESS_TEST(MissingBootCountFailsFsRoot)
READINESS_TEST(RadioSilenceWarnsOnlyAfterQuiescence)
READINESS_TEST(BatteryMissingFailsAndLowWarns)
READINESS_TEST(LowFreeSpaceWarns)
READINESS_TEST(BootTriggersOneFullRun)
READINESS_TEST(PingIsAnswered)

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
