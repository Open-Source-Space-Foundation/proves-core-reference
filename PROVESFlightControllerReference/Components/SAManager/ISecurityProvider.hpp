// ======================================================================
// \title  ISecurityProvider.hpp
// \brief  Abstract crypto-provider interface called by Svc::Ccsds::SAManager
//
// Projects implement this interface to plug in their own header parsing
// and MAC verification logic. SAManager handles SA lookup and anti-replay;
// the provider handles everything that touches key material or wire layout.
// ======================================================================

#ifndef Svc_Ccsds_ISecurityProvider_HPP
#define Svc_Ccsds_ISecurityProvider_HPP

#include "FpConfig.hpp"
#include "Fw/Buffer/Buffer.hpp"

namespace Svc {
namespace Ccsds {

//! Header offsets and identifying fields reported by ISecurityProvider::parseHeader.
struct SecurityHeaderInfo {
    //! Security Parameter Index (CCSDS 355.0-B-2 §3.3.2). 32-bit holds any SA-defined width.
    U32 spi = 0;
    //! Anti-replay sequence number from the Security Header (§3.3.2.5).
    U32 sequenceNumber = 0;
    //! Bytes from start of payload buffer to end of Security Header
    //! (== ProcessSecurity Return offset).
    FwSizeType headerSize = 0;
    //! Bytes occupied by the Security Trailer at the end of the payload buffer.
    FwSizeType trailerSize = 0;
};

//! Abstract crypto-provider interface. SAManager calls these methods
//! synchronously from its processSecurityIn handler. Implementations must
//! be reentrant if SAManager is called from multiple threads.
class ISecurityProvider {
  public:
    virtual ~ISecurityProvider() = default;

    //! Parse the Security Header at the start of `payload`. On success, populate
    //! `outInfo` with SPI, sequence number, and the header/trailer sizes.
    //! \param payload Buffer containing [Security Header | Frame Data | Security Trailer]
    //! \param outInfo Filled on success.
    //! \return true on success, false on parse failure.
    virtual bool parseHeader(const Fw::Buffer& payload, SecurityHeaderInfo& outInfo) = 0;

    //! Verify the MAC over the authenticated region of `payload`. The provider
    //! looks up the key material by SPI through its own internal state.
    //! The authenticated region is implementation-defined but typically covers
    //! the slice `[0, payload.getSize() - info.trailerSize)`.
    //! \param payload Buffer to verify.
    //! \param info Output from a successful parseHeader() call on the same buffer.
    //! \return 0 on success (MAC valid), non-zero on failure. Non-zero values
    //!         are reported to ground in the ProviderError event.
    virtual I32 verifyMac(const Fw::Buffer& payload, const SecurityHeaderInfo& info) = 0;
};

}  // namespace Ccsds
}  // namespace Svc

#endif
