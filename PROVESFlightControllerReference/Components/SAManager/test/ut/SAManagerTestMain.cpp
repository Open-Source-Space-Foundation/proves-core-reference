// ======================================================================
// \title  SAManagerTestMain.cpp
// \brief  test main function for SAManager
// ======================================================================

#include "SAManagerTester.hpp"

#define ITER(name)                          \
    TEST(SAManager, name) {                 \
        Svc::Ccsds::SAManagerTester tester; \
        tester.name();                      \
    }

ITER(testRegisterSaOk)
ITER(testRegisterSaTableFull)
ITER(testRegisterDuplicateSpiRejected)
ITER(testRegisterInvalidWindowSizeRejected)
ITER(testRemoveSa)
ITER(testProviderUnconnectedReturnsInternalError)
ITER(testNominalAccept)
ITER(testParseFailureReturnsInternalError)
ITER(testInvalidSpiReturnsInvalidSpi)
ITER(testAntiReplayDuplicateRejected)
ITER(testAntiReplayOutsideWindowRejected)
ITER(testAntiReplayForwardJumpUpdatesWindow)
ITER(testMacFailureDoesNotAdvanceWindow)
ITER(testWraparoundAcceptsForwardAcrossZero)
ITER(testWildcardScidMatchesAny)
ITER(testWildcardVcidMatchesAny)

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
