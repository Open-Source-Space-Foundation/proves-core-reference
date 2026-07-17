// ======================================================================
// \title  SAManager.hpp
// \brief  hpp file for SAManager component implementation class
// ======================================================================

#ifndef Svc_Ccsds_SAManager_HPP
#define Svc_Ccsds_SAManager_HPP

#include "Svc/Ccsds/SAManager/ISecurityProvider.hpp"
#include "Svc/Ccsds/SAManager/SAManagerComponentAc.hpp"

namespace Svc {
namespace Ccsds {

//! A Security Association record. Stored in the SAManager SA table.
//!
//! `scope` describes which GVCID this SA applies to. Use the wildcard sentinel
//! WILDCARD_VCID for vcid (or WILDCARD_SCID for scid) to match any value.
//!
//! `windowSize` is the anti-replay window width (max 64 to fit the bitmap).
//! `antiReplayBitmap` bit i set => `highestAcceptedSeq - i` has been accepted.
struct SecurityAssociation {
    static constexpr U16 WILDCARD_SCID = 0xFFFF;
    static constexpr U8 WILDCARD_VCID = 0xFF;

    U32 spi = 0;
    GVCID scope;
    U32 highestAcceptedSeq = 0;
    U64 antiReplayBitmap = 0;
    U16 windowSize = 64;
    bool active = false;
};

class SAManager final : public SAManagerComponentBase {
    friend class SAManagerTester;

  public:
    //! Maximum number of Security Associations the table can hold.
    static constexpr U16 MAX_SAS = 8;

    //! Anti-replay bitmap width. Cannot exceed 64 (bitmap is U64).
    static constexpr U16 MAX_WINDOW_SIZE = 64;

    //! Construct SAManager object
    SAManager(const char* const compName);

    //! Destroy SAManager object
    ~SAManager() override = default;

    //! Bind the crypto provider. MUST be called before processSecurityIn fires.
    //! Caller retains ownership; provider must outlive this component.
    void setProvider(ISecurityProvider* provider) { this->m_provider = provider; }

    //! Register an SA. Returns true on success. Returns false (and emits
    //! SaTableFull or no event for duplicates) if the table is full or an
    //! active SA with the same SPI is already present.
    bool registerSa(const SecurityAssociation& sa);

    //! Remove an SA by SPI. Returns true if an SA was removed.
    bool removeSa(U32 spi);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    ProcessSecurityResult processSecurityIn_handler(FwIndexType portNum,
                                                    const Ccsds::GVCID& gvcid,
                                                    Fw::Buffer& payload) override;

    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Find an active SA matching `spi` and `gvcid` (honoring wildcard scope).
    //! Returns nullptr if not found.
    SecurityAssociation* findSa(U32 spi, const GVCID& gvcid);

    //! Anti-replay check. If `seq` is acceptable, updates the SA's window
    //! state and returns true. Otherwise returns false without changing state.
    bool antiReplayAcceptAndUpdate(SecurityAssociation& sa, U32 seq);

    //! Bump rejected counter + telemetry.
    void bumpRejected();

    //! Bump accepted counter + telemetry.
    void bumpAccepted();

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    ISecurityProvider* m_provider = nullptr;
    SecurityAssociation m_sas[MAX_SAS];
    U16 m_saCount = 0;
    U32 m_accepted = 0;
    U32 m_rejected = 0;
};

}  // namespace Ccsds
}  // namespace Svc

#endif
