// ======================================================================
// \title  SAManagerTester.cpp
// \brief  cpp file for SAManager component test harness
// ======================================================================

#include "SAManagerTester.hpp"

namespace Svc {
namespace Ccsds {

SAManagerTester::SAManagerTester() : SAManagerGTestBase("SAManagerTester", MAX_HISTORY_SIZE), component("SAManager") {
    this->initComponents();
    this->connectPorts();
    this->component.setProvider(this);
}

// ----------------------------------------------------------------------
// ISecurityProvider stub
// ----------------------------------------------------------------------

bool SAManagerTester::parseHeader(const Fw::Buffer& /*payload*/, SecurityHeaderInfo& outInfo) {
    this->m_parseHeaderCalls++;
    outInfo = this->m_nextHeaderInfo;
    return this->m_nextParseOk;
}

I32 SAManagerTester::verifyMac(const Fw::Buffer& /*payload*/, const SecurityHeaderInfo& /*info*/) {
    this->m_verifyMacCalls++;
    return this->m_nextMacRc;
}

void SAManagerTester::scriptParse(U32 spi, U32 seq, FwSizeType headerSize, FwSizeType trailerSize) {
    this->m_nextParseOk = true;
    this->m_nextHeaderInfo.spi = spi;
    this->m_nextHeaderInfo.sequenceNumber = seq;
    this->m_nextHeaderInfo.headerSize = headerSize;
    this->m_nextHeaderInfo.trailerSize = trailerSize;
    this->m_nextMacRc = 0;
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

namespace {
SecurityAssociation makeSa(U32 spi, U16 scid, U8 vcid, U16 windowSize = 64, U32 highestSeq = 0) {
    SecurityAssociation sa;
    sa.spi = spi;
    sa.scope.set_tfvn(0);
    sa.scope.set_scid(scid);
    sa.scope.set_vcid(vcid);
    sa.windowSize = windowSize;
    sa.highestAcceptedSeq = highestSeq;
    sa.antiReplayBitmap = 0;
    sa.active = false;
    return sa;
}

GVCID makeGvcid(U16 scid, U8 vcid) {
    GVCID g;
    g.set_tfvn(0);
    g.set_scid(scid);
    g.set_vcid(vcid);
    return g;
}
}  // namespace

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void SAManagerTester::testRegisterSaOk() {
    ASSERT_TRUE(this->component.registerSa(makeSa(0x1, 0x44, 0)));
    ASSERT_EVENTS_SaRegistered_SIZE(1);
    ASSERT_EVENTS_SaRegistered(0, 0x1u, 0x44, 0);
    ASSERT_TLM_SACount(0, 1);
}

void SAManagerTester::testRegisterSaTableFull() {
    for (U16 i = 0; i < SAManager::MAX_SAS; i++) {
        ASSERT_TRUE(this->component.registerSa(makeSa(static_cast<U32>(i + 1), 0x44, 0)));
    }
    ASSERT_FALSE(this->component.registerSa(makeSa(99, 0x44, 0)));
    ASSERT_EVENTS_SaTableFull_SIZE(1);
}

void SAManagerTester::testRegisterDuplicateSpiRejected() {
    ASSERT_TRUE(this->component.registerSa(makeSa(0x1, 0x44, 0)));
    ASSERT_FALSE(this->component.registerSa(makeSa(0x1, 0x44, 1)));
    ASSERT_EVENTS_SaRegistered_SIZE(1);  // only the first emitted
}

void SAManagerTester::testRegisterInvalidWindowSizeRejected() {
    SecurityAssociation sa = makeSa(0x1, 0x44, 0);
    sa.windowSize = 0;
    ASSERT_FALSE(this->component.registerSa(sa));
    sa.windowSize = SAManager::MAX_WINDOW_SIZE + 1;
    ASSERT_FALSE(this->component.registerSa(sa));
}

void SAManagerTester::testRemoveSa() {
    ASSERT_TRUE(this->component.registerSa(makeSa(0x1, 0x44, 0)));
    ASSERT_TRUE(this->component.removeSa(0x1));
    ASSERT_FALSE(this->component.removeSa(0x1));                    // already gone
    ASSERT_TRUE(this->component.registerSa(makeSa(0x1, 0x44, 1)));  // slot reusable
}

void SAManagerTester::testProviderUnconnectedReturnsInternalError() {
    this->component.setProvider(nullptr);
    Fw::Buffer buf(this->m_payloadBuf, 16);
    GVCID g = makeGvcid(0x44, 0);
    ProcessSecurityResult r = this->invoke_to_processSecurityIn(0, g, buf);
    ASSERT_EQ(r.get_status(), VerificationStatus::FAILURE);
    ASSERT_EQ(r.get_statusCode(), VerificationStatusCode::INTERNAL_ERROR);
}

void SAManagerTester::testNominalAccept() {
    ASSERT_TRUE(this->component.registerSa(makeSa(0x10, 0x44, 0)));
    this->scriptParse(0x10, 1, /*headerSize=*/6, /*trailerSize=*/16);
    Fw::Buffer buf(this->m_payloadBuf, 64);
    GVCID g = makeGvcid(0x44, 0);

    ProcessSecurityResult r = this->invoke_to_processSecurityIn(0, g, buf);
    ASSERT_EQ(r.get_status(), VerificationStatus::NO_FAILURE);
    ASSERT_EQ(r.get_statusCode(), VerificationStatusCode::NONE);
    ASSERT_EQ(r.get_returnOffset(), 6u);
    ASSERT_EQ(r.get_returnSize(), 64u - 6 - 16);
    ASSERT_EQ(this->m_parseHeaderCalls, 1u);
    ASSERT_EQ(this->m_verifyMacCalls, 1u);
}

void SAManagerTester::testParseFailureReturnsInternalError() {
    ASSERT_TRUE(this->component.registerSa(makeSa(0x10, 0x44, 0)));
    this->m_nextParseOk = false;
    Fw::Buffer buf(this->m_payloadBuf, 16);
    GVCID g = makeGvcid(0x44, 0);
    ProcessSecurityResult r = this->invoke_to_processSecurityIn(0, g, buf);
    ASSERT_EQ(r.get_statusCode(), VerificationStatusCode::INTERNAL_ERROR);
    ASSERT_EQ(this->m_verifyMacCalls, 0u);  // MAC not attempted on parse failure
}

void SAManagerTester::testInvalidSpiReturnsInvalidSpi() {
    // No SA registered.
    this->scriptParse(0x999, 1);
    Fw::Buffer buf(this->m_payloadBuf, 16);
    GVCID g = makeGvcid(0x44, 0);
    ProcessSecurityResult r = this->invoke_to_processSecurityIn(0, g, buf);
    ASSERT_EQ(r.get_statusCode(), VerificationStatusCode::INVALID_SPI);
    ASSERT_EVENTS_SaLookupFailed_SIZE(1);
}

void SAManagerTester::testAntiReplayDuplicateRejected() {
    ASSERT_TRUE(this->component.registerSa(makeSa(0x10, 0x44, 0)));
    Fw::Buffer buf(this->m_payloadBuf, 64);
    GVCID g = makeGvcid(0x44, 0);

    this->scriptParse(0x10, 5, 6, 16);
    ProcessSecurityResult r1 = this->invoke_to_processSecurityIn(0, g, buf);
    ASSERT_EQ(r1.get_status(), VerificationStatus::NO_FAILURE);

    // Replay seq=5
    this->scriptParse(0x10, 5, 6, 16);
    ProcessSecurityResult r2 = this->invoke_to_processSecurityIn(0, g, buf);
    ASSERT_EQ(r2.get_statusCode(), VerificationStatusCode::ANTI_REPLAY_SEQUENCE_FAILURE);
    ASSERT_EVENTS_AntiReplayReject_SIZE(1);
}

void SAManagerTester::testAntiReplayOutsideWindowRejected() {
    // window=4: anything > 4 behind the highest is rejected
    ASSERT_TRUE(this->component.registerSa(makeSa(0x10, 0x44, 0, /*window=*/4)));
    Fw::Buffer buf(this->m_payloadBuf, 64);
    GVCID g = makeGvcid(0x44, 0);

    this->scriptParse(0x10, 100, 6, 16);
    this->invoke_to_processSecurityIn(0, g, buf);  // highestAccepted -> 100

    this->scriptParse(0x10, 90, 6, 16);  // 10 behind, window only covers 4
    ProcessSecurityResult r = this->invoke_to_processSecurityIn(0, g, buf);
    ASSERT_EQ(r.get_statusCode(), VerificationStatusCode::ANTI_REPLAY_SEQUENCE_FAILURE);
}

void SAManagerTester::testAntiReplayForwardJumpUpdatesWindow() {
    ASSERT_TRUE(this->component.registerSa(makeSa(0x10, 0x44, 0)));
    Fw::Buffer buf(this->m_payloadBuf, 64);
    GVCID g = makeGvcid(0x44, 0);

    this->scriptParse(0x10, 10, 6, 16);
    ASSERT_EQ(this->invoke_to_processSecurityIn(0, g, buf).get_status(), VerificationStatus::NO_FAILURE);
    this->scriptParse(0x10, 12, 6, 16);
    ASSERT_EQ(this->invoke_to_processSecurityIn(0, g, buf).get_status(), VerificationStatus::NO_FAILURE);
    // Now check that seq=11 (within window, behind 12) is still acceptable
    this->scriptParse(0x10, 11, 6, 16);
    ASSERT_EQ(this->invoke_to_processSecurityIn(0, g, buf).get_status(), VerificationStatus::NO_FAILURE);
    // ...and re-sending 11 is a duplicate
    this->scriptParse(0x10, 11, 6, 16);
    ASSERT_EQ(this->invoke_to_processSecurityIn(0, g, buf).get_statusCode(),
              VerificationStatusCode::ANTI_REPLAY_SEQUENCE_FAILURE);
}

void SAManagerTester::testMacFailureDoesNotAdvanceWindow() {
    ASSERT_TRUE(this->component.registerSa(makeSa(0x10, 0x44, 0)));
    Fw::Buffer buf(this->m_payloadBuf, 64);
    GVCID g = makeGvcid(0x44, 0);

    // Accept seq=5
    this->scriptParse(0x10, 5, 6, 16);
    this->invoke_to_processSecurityIn(0, g, buf);

    // Spoofed seq=100 with bad MAC: window must NOT advance to 100
    this->scriptParse(0x10, 100, 6, 16);
    this->m_nextMacRc = -1;
    ProcessSecurityResult bad = this->invoke_to_processSecurityIn(0, g, buf);
    ASSERT_EQ(bad.get_statusCode(), VerificationStatusCode::MAC_VERIFICATION_FAILURE);

    // Legitimate seq=6 should still be accepted (window stayed at 5)
    this->scriptParse(0x10, 6, 6, 16);
    this->m_nextMacRc = 0;
    ProcessSecurityResult ok = this->invoke_to_processSecurityIn(0, g, buf);
    ASSERT_EQ(ok.get_status(), VerificationStatus::NO_FAILURE);
}

void SAManagerTester::testWraparoundAcceptsForwardAcrossZero() {
    SecurityAssociation sa = makeSa(0x10, 0x44, 0, /*window=*/64, /*highestSeq=*/0xFFFFFFFE);
    sa.antiReplayBitmap = 1;  // mark 0xFFFFFFFE as accepted so init logic doesn't fire
    ASSERT_TRUE(this->component.registerSa(sa));
    Fw::Buffer buf(this->m_payloadBuf, 64);
    GVCID g = makeGvcid(0x44, 0);

    this->scriptParse(0x10, 0xFFFFFFFF, 6, 16);
    ASSERT_EQ(this->invoke_to_processSecurityIn(0, g, buf).get_status(), VerificationStatus::NO_FAILURE);
    this->scriptParse(0x10, 0, 6, 16);
    ASSERT_EQ(this->invoke_to_processSecurityIn(0, g, buf).get_status(), VerificationStatus::NO_FAILURE);
    this->scriptParse(0x10, 1, 6, 16);
    ASSERT_EQ(this->invoke_to_processSecurityIn(0, g, buf).get_status(), VerificationStatus::NO_FAILURE);
}

void SAManagerTester::testWildcardScidMatchesAny() {
    SecurityAssociation sa = makeSa(0x10, SecurityAssociation::WILDCARD_SCID, 0);
    ASSERT_TRUE(this->component.registerSa(sa));
    this->scriptParse(0x10, 1, 6, 16);
    Fw::Buffer buf(this->m_payloadBuf, 64);
    GVCID g1 = makeGvcid(0xABC, 0);
    ASSERT_EQ(this->invoke_to_processSecurityIn(0, g1, buf).get_status(), VerificationStatus::NO_FAILURE);
}

void SAManagerTester::testWildcardVcidMatchesAny() {
    SecurityAssociation sa = makeSa(0x10, 0x44, SecurityAssociation::WILDCARD_VCID);
    ASSERT_TRUE(this->component.registerSa(sa));
    this->scriptParse(0x10, 1, 6, 16);
    Fw::Buffer buf(this->m_payloadBuf, 64);
    GVCID g1 = makeGvcid(0x44, 17);
    ASSERT_EQ(this->invoke_to_processSecurityIn(0, g1, buf).get_status(), VerificationStatus::NO_FAILURE);
}

}  // namespace Ccsds
}  // namespace Svc
