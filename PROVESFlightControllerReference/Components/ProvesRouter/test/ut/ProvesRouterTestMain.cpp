// ======================================================================
// \title  ProvesRouterTestMain.cpp
// \brief  cpp file for ProvesRouter component test main function
// ======================================================================

#include "ProvesRouterTester.hpp"

TEST(Nominal, AuthenticatedCommandIsRouted) {
    Svc::ProvesRouterTester tester;
    tester.testAuthenticatedCommandIsRouted();
}

TEST(Nominal, AuthenticatedFileIsRoutedAsCopy) {
    Svc::ProvesRouterTester tester;
    tester.testAuthenticatedFileIsRoutedAsCopy();
}

TEST(Nominal, AuthenticatedUnknownIsRoutedAsCopy) {
    Svc::ProvesRouterTester tester;
    tester.testAuthenticatedUnknownIsRoutedAsCopy();
}

TEST(Nominal, FileBufferReturnIsDeallocated) {
    Svc::ProvesRouterTester tester;
    tester.testFileBufferReturnIsDeallocated();
}

TEST(Nominal, CountersAccumulate) {
    Svc::ProvesRouterTester tester;
    tester.testCountersAccumulate();
}

TEST(Security, UnauthenticatedBypassCommandIsRouted) {
    Svc::ProvesRouterTester tester;
    tester.testUnauthenticatedBypassCommandIsRouted();
}

TEST(Security, UnauthenticatedCommandIsRejected) {
    Svc::ProvesRouterTester tester;
    tester.testUnauthenticatedCommandIsRejected();
}

TEST(Security, UnauthenticatedShortCommandIsRejected) {
    Svc::ProvesRouterTester tester;
    tester.testUnauthenticatedShortCommandIsRejected();
}

TEST(Security, UnauthenticatedFileWithBypassOpCodeIsRejected) {
    Svc::ProvesRouterTester tester;
    tester.testUnauthenticatedFileWithBypassOpCodeIsRejected();
}

TEST(Security, UnauthenticatedUnknownWithBypassOpCodeIsRejected) {
    Svc::ProvesRouterTester tester;
    tester.testUnauthenticatedUnknownWithBypassOpCodeIsRejected();
}

TEST(Error, OversizeCommandEmitsSerializationError) {
    Svc::ProvesRouterTester tester;
    tester.testOversizeCommandEmitsSerializationError();
}

TEST(Error, FileAllocationFailure) {
    Svc::ProvesRouterTester tester;
    tester.testFileAllocationFailure();
}

TEST(Error, UnknownAllocationFailure) {
    Svc::ProvesRouterTester tester;
    tester.testUnknownAllocationFailure();
}

TEST(Error, ShortAllocationIsDeallocated) {
    Svc::ProvesRouterTester tester;
    tester.testShortAllocationIsDeallocated();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
