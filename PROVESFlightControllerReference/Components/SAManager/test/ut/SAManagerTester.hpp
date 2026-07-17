// ======================================================================
// \title  SAManagerTester.hpp
// \brief  hpp file for SAManager component test harness
// ======================================================================

#ifndef Svc_Ccsds_SAManagerTester_HPP
#define Svc_Ccsds_SAManagerTester_HPP

#include "Svc/Ccsds/SAManager/SAManager.hpp"
#include "Svc/Ccsds/SAManager/SAManagerGTestBase.hpp"

namespace Svc {
namespace Ccsds {

class SAManagerTester final : public SAManagerGTestBase, public ISecurityProvider {
  public:
    static const FwSizeType MAX_HISTORY_SIZE = 64;
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    SAManagerTester();
    ~SAManagerTester() = default;

    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    void testRegisterSaOk();
    void testRegisterSaTableFull();
    void testRegisterDuplicateSpiRejected();
    void testRegisterInvalidWindowSizeRejected();
    void testRemoveSa();
    void testProviderUnconnectedReturnsInternalError();
    void testNominalAccept();
    void testParseFailureReturnsInternalError();
    void testInvalidSpiReturnsInvalidSpi();
    void testAntiReplayDuplicateRejected();
    void testAntiReplayOutsideWindowRejected();
    void testAntiReplayForwardJumpUpdatesWindow();
    void testMacFailureDoesNotAdvanceWindow();
    void testWraparoundAcceptsForwardAcrossZero();
    void testWildcardScidMatchesAny();
    void testWildcardVcidMatchesAny();

    // ----------------------------------------------------------------------
    // ISecurityProvider stub (scripted by individual tests)
    // ----------------------------------------------------------------------

    bool parseHeader(const Fw::Buffer& payload, SecurityHeaderInfo& outInfo) override;
    I32 verifyMac(const Fw::Buffer& payload, const SecurityHeaderInfo& info) override;

  private:
    // Scripted scenarios for the next ISecurityProvider call
    bool m_nextParseOk = true;
    SecurityHeaderInfo m_nextHeaderInfo{};
    I32 m_nextMacRc = 0;
    U32 m_parseHeaderCalls = 0;
    U32 m_verifyMacCalls = 0;

    void scriptParse(U32 spi, U32 seq, FwSizeType headerSize = 0, FwSizeType trailerSize = 0);

    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    void connectPorts();
    void initComponents();

    SAManager component;

    U8 m_payloadBuf[128] = {};
};

}  // namespace Ccsds
}  // namespace Svc

#endif
