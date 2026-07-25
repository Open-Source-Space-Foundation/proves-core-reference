// ======================================================================
// \title  TlmArchive.hpp
// \author aychar
// \brief  hpp file for TlmArchive component implementation class
// ======================================================================

#ifndef Components_TlmArchive_HPP
#define Components_TlmArchive_HPP

#include "PROVESFlightControllerReference/Components/TlmArchive/TlmArchiveComponentAc.hpp"

namespace Components {

class TlmArchive final : public TlmArchiveComponentBase {
  public:
    //! Construct TlmArchive object
    TlmArchive(const char* const compName  //!< The component name
    );

    //! Destroy TlmArchive object
    ~TlmArchive();

  private:
    void comIn_handler(FwIndexType portNum, Fw::ComBuffer& data, U32 context) override;
};

}  // namespace Components

#endif
