// ======================================================================
// \title  SAManager.cpp
// \brief  cpp file for SAManager component implementation class
// ======================================================================

#include "Svc/Ccsds/SAManager/SAManager.hpp"

namespace Svc {
namespace Ccsds {

namespace {

//! Result of an anti-replay check. When `accept` is true, callers may pass
//! `newBitmap` and `newHighest` to commit the state update.
struct AntiReplayDecision {
    bool accept;
    U64 newBitmap;
    U32 newHighest;
};

//! Anti-replay check using a sliding-window bitmap. Bit i of the bitmap means
//! `highestAcceptedSeq - i` has been accepted. Handles U32 wraparound via
//! signed difference. No state is mutated by this function.
AntiReplayDecision checkAntiReplay(const SecurityAssociation& sa, U32 seq) {
    AntiReplayDecision d{false, sa.antiReplayBitmap, sa.highestAcceptedSeq};

    // First-frame case: SA registered with default state, no frames seen yet.
    if (sa.antiReplayBitmap == 0 && sa.highestAcceptedSeq == 0) {
        d.accept = true;
        d.newBitmap = 1;
        d.newHighest = seq;
        return d;
    }

    const I32 diff = static_cast<I32>(seq - sa.highestAcceptedSeq);
    if (diff > 0) {
        // Forward jump: advance window by `diff`, set the low bit for `seq`.
        const U32 shift = static_cast<U32>(diff);
        d.accept = true;
        d.newBitmap = (shift >= 64) ? static_cast<U64>(1) : ((sa.antiReplayBitmap << shift) | static_cast<U64>(1));
        d.newHighest = seq;
        return d;
    }
    if (diff == 0) {
        return d;  // duplicate of highestAccepted
    }
    // Backward: seq is older than highestAccepted.
    const U32 backwardBits = static_cast<U32>(-diff);
    if (backwardBits >= sa.windowSize) {
        return d;  // too old, outside window
    }
    const U64 bitMask = static_cast<U64>(1) << backwardBits;
    if ((sa.antiReplayBitmap & bitMask) != 0) {
        return d;  // duplicate within window
    }
    d.accept = true;
    d.newBitmap = sa.antiReplayBitmap | bitMask;
    d.newHighest = sa.highestAcceptedSeq;
    return d;
}

bool gvcidMatches(const GVCID& scope, const GVCID& incoming) {
    if (scope.get_tfvn() != incoming.get_tfvn()) {
        return false;
    }
    if (scope.get_scid() != SecurityAssociation::WILDCARD_SCID && scope.get_scid() != incoming.get_scid()) {
        return false;
    }
    if (scope.get_vcid() != SecurityAssociation::WILDCARD_VCID && scope.get_vcid() != incoming.get_vcid()) {
        return false;
    }
    return true;
}

}  // namespace

// ----------------------------------------------------------------------
// Construction
// ----------------------------------------------------------------------

SAManager::SAManager(const char* const compName) : SAManagerComponentBase(compName) {}

// ----------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------

bool SAManager::registerSa(const SecurityAssociation& sa) {
    if (sa.windowSize == 0 || sa.windowSize > MAX_WINDOW_SIZE) {
        return false;
    }
    // Reject duplicates of any active SA with the same SPI.
    for (U16 i = 0; i < MAX_SAS; i++) {
        if (this->m_sas[i].active && this->m_sas[i].spi == sa.spi) {
            return false;
        }
    }
    // Find a free slot.
    for (U16 i = 0; i < MAX_SAS; i++) {
        if (not this->m_sas[i].active) {
            this->m_sas[i] = sa;
            this->m_sas[i].active = true;
            this->m_saCount++;
            this->tlmWrite_SACount(this->m_saCount);
            this->log_ACTIVITY_LO_SaRegistered(sa.spi, sa.scope.get_scid(), sa.scope.get_vcid());
            return true;
        }
    }
    // Table full.
    this->log_WARNING_HI_SaTableFull(sa.spi);
    return false;
}

bool SAManager::removeSa(U32 spi) {
    for (U16 i = 0; i < MAX_SAS; i++) {
        if (this->m_sas[i].active && this->m_sas[i].spi == spi) {
            this->m_sas[i] = SecurityAssociation{};  // reset to default
            this->m_saCount--;
            this->tlmWrite_SACount(this->m_saCount);
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------
// Handler implementations
// ----------------------------------------------------------------------

ProcessSecurityResult SAManager::processSecurityIn_handler(FwIndexType /*portNum*/,
                                                           const Ccsds::GVCID& gvcid,
                                                           Fw::Buffer& payload) {
    ProcessSecurityResult res;  // default: FAILURE / INTERNAL_ERROR

    if (this->m_provider == nullptr) {
        this->log_WARNING_HI_ProviderError(0, -1);
        this->bumpRejected();
        return res;
    }

    SecurityHeaderInfo info;
    if (not this->m_provider->parseHeader(payload, info)) {
        this->log_WARNING_HI_ProviderError(0, -2);
        // res already FAILURE/INTERNAL_ERROR
        this->bumpRejected();
        return res;
    }

    SecurityAssociation* sa = this->findSa(info.spi, gvcid);
    if (sa == nullptr) {
        this->log_WARNING_LO_SaLookupFailed(info.spi);
        res.set_statusCode(VerificationStatusCode::INVALID_SPI);
        this->bumpRejected();
        return res;
    }

    // Anti-replay check (read-only — state is updated after MAC verifies).
    const AntiReplayDecision decision = checkAntiReplay(*sa, info.sequenceNumber);
    if (not decision.accept) {
        this->log_WARNING_LO_AntiReplayReject(info.spi, info.sequenceNumber, sa->highestAcceptedSeq);
        res.set_statusCode(VerificationStatusCode::ANTI_REPLAY_SEQUENCE_FAILURE);
        this->bumpRejected();
        return res;
    }

    // Verify MAC.
    const I32 rc = this->m_provider->verifyMac(payload, info);
    if (rc != 0) {
        this->log_WARNING_HI_ProviderError(info.spi, rc);
        res.set_statusCode(VerificationStatusCode::MAC_VERIFICATION_FAILURE);
        this->bumpRejected();
        return res;
    }

    // Both checks passed: commit the anti-replay state update.
    sa->antiReplayBitmap = decision.newBitmap;
    sa->highestAcceptedSeq = decision.newHighest;

    // Compute the return slice. Defend against provider reporting impossible sizes.
    const FwSizeType payloadSize = static_cast<FwSizeType>(payload.getSize());
    if (info.headerSize > payloadSize || info.trailerSize > (payloadSize - info.headerSize)) {
        this->log_WARNING_HI_ProviderError(info.spi, -3);
        res.set_statusCode(VerificationStatusCode::INTERNAL_ERROR);
        this->bumpRejected();
        return res;
    }
    res.set_status(VerificationStatus::NO_FAILURE);
    res.set_statusCode(VerificationStatusCode::NONE);
    res.set_returnOffset(info.headerSize);
    res.set_returnSize(payloadSize - info.headerSize - info.trailerSize);
    this->bumpAccepted();
    return res;
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

SecurityAssociation* SAManager::findSa(U32 spi, const GVCID& gvcid) {
    for (U16 i = 0; i < MAX_SAS; i++) {
        if (this->m_sas[i].active && this->m_sas[i].spi == spi && gvcidMatches(this->m_sas[i].scope, gvcid)) {
            return &this->m_sas[i];
        }
    }
    return nullptr;
}

void SAManager::bumpAccepted() {
    this->m_accepted++;
    this->tlmWrite_AcceptedTotal(this->m_accepted);
}

void SAManager::bumpRejected() {
    this->m_rejected++;
    this->tlmWrite_RejectedTotal(this->m_rejected);
}

}  // namespace Ccsds
}  // namespace Svc
