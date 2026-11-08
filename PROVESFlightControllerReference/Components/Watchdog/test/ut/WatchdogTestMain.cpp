// ======================================================================
// \title  WatchdogTestMain.cpp
// \author ncc-michael
// \brief  cpp file for Watchdog component test main function
// ======================================================================

#include "WatchdogTester.hpp"

TEST(Nominal, RunTogglesGpioWhileStarted) {
    Components::WatchdogTester tester;
    tester.testRunTogglesGpioWhileStarted();
}

TEST(Nominal, StartCommand) {
    Components::WatchdogTester tester;
    tester.testStartCommand();
}

TEST(Nominal, StopCommand) {
    Components::WatchdogTester tester;
    tester.testStopCommand();
}

TEST(Nominal, StartStopSignalPorts) {
    Components::WatchdogTester tester;
    tester.testStartStopSignalPorts();
}

TEST(Nominal, TransitionCountAcrossStopStart) {
    Components::WatchdogTester tester;
    tester.testTransitionCountAcrossStopStart();
}

TEST(Error, GpioFailureIsTolerated) {
    Components::WatchdogTester tester;
    tester.testGpioFailureIsTolerated();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
