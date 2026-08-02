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
#include "PROVESFlightControllerReference/project/config/TlmPacketizerCfg.hpp"

namespace Components {

class TlmArchive final : public TlmArchiveComponentBase {
  public:
    //! Construct TlmArchive object
    TlmArchive(const char* const compName  //!< The component name
    );

    //! Destroy TlmArchive object
    ~TlmArchive();

  private:
    static constexpr FwSizeType PACKET_QUEUE_CAPACITY = Svc::MAX_PACKETIZER_PACKETS;
    static_assert(PACKET_QUEUE_CAPACITY > 0, "Telemetry archive packet queue must have storage");

    void comIn_handler(FwIndexType portNum, Fw::ComBuffer& data, U32 context) override;
    void run_handler(FwIndexType portNum, U32 context) override;

    Os::Mutex m_queueMutex;
    Fw::ComBuffer m_packetQueue[PACKET_QUEUE_CAPACITY];
    FwSizeType m_queueHead = 0;
    FwSizeType m_queueTail = 0;
    FwSizeType m_queueSize = 0;
    std::atomic<U32> m_fileSize{0};
    std::atomic<int> m_failures{0};
    std::atomic<bool> m_antennasDeployed{false};
    bool m_directoryInitialized = false;
    bool m_fileSizeInitialized = false;
    bool m_queueWasEmpty = false;
};

}  // namespace Components

#endif
