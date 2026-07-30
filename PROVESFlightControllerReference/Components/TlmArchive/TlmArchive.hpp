// ======================================================================
// \title  TlmArchive.hpp
// \author aychar
// \brief  hpp file for TlmArchive component implementation class
// ======================================================================

#ifndef Components_TlmArchive_HPP
#define Components_TlmArchive_HPP

#include <atomic>

#include "Os/Mutex.hpp"
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
    void run_handler(FwIndexType portNum, U32 context) override;

    Os::Mutex m_queueMutex;
    Fw::ComBuffer m_pendingPacket;
    std::atomic<U32> m_fileSize{0};
    std::atomic<int> m_failures{0};
    std::atomic<bool> m_antennasDeployed{false};
    bool m_directoryInitialized = false;
    bool m_fileSizeInitialized = false;
    bool m_packetPending = false;
};

}  // namespace Components

#endif
