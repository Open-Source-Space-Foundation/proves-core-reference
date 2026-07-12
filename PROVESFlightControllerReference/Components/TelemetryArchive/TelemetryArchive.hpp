#ifndef Components_TelemetryArchive_HPP
#define Components_TelemetryArchive_HPP

#include "PROVESFlightControllerReference/Components/TelemetryArchive/TelemetryArchiveComponentAc.hpp"

#include "Os/File.hpp"
#include "Os/Mutex.hpp"

namespace Components {

class TelemetryArchive final : public TelemetryArchiveComponentBase {
  public:
    explicit TelemetryArchive(const char* const compName);
    ~TelemetryArchive();
    void init(FwEnumStoreType instance = 0);

  private:
    static constexpr U32 SEGMENT_SIZE = 256U * 1024U;
    static constexpr U32 DEPLOYMENT_BUDGET = 1024U * 1024U;
    static constexpr U32 ROLLING_BUDGET = 4U * 1024U * 1024U;
    static constexpr U32 MIN_FREE_BYTES = 1024U * 1024U;
    static constexpr U32 NORMAL_SYNC_RECORDS = 16U;
    static constexpr U32 PATH_SIZE = 96U;

    void comIn_handler(FwIndexType portNum, Fw::ComBuffer& data, U32 context) override;
    void deploymentComplete_handler(FwIndexType portNum) override;
    void ENABLE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void DISABLE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void CLOSE_SEGMENT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void PURGE_ROLLING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void GET_STATUS_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    bool readDeploymentState() const;
    bool ensureDirectories();
    bool openSegment();
    void closeSegment(bool emitEvent = true);
    bool writeRecord(const Fw::ComBuffer& data);
    bool ensureCapacity();
    U32 scanDirectory(const char* directory, const char* prefix, U32& nextSequence, U32* oldestSequence = nullptr);
    bool pruneOldestRolling(U32& usage);
    void pause(const char* reason);
    void publishState();
    static U32 crc32(const U8* data, FwSizeType size);
    static void putU16(U8* destination, U16 value);
    static void putU32(U8* destination, U32 value);

    Os::Mutex m_mutex;
    Os::File m_file;
    bool m_enabled = true;
    bool m_paused = false;
    bool m_deploymentMode = true;
    bool m_fileOpen = false;
    U32 m_segmentBytes = 0;
    U32 m_segmentSequence = 0;
    U32 m_recordSequence = 0;
    U32 m_recordsSinceSync = 0;
    U32 m_recordsStored = 0;
    U32 m_bytesStored = 0;
    U32 m_recordsDropped = 0;
    U32 m_rollingBytesPruned = 0;
    U32 m_maxWriteDurationUs = 0;
    char m_fileName[PATH_SIZE] = {};
};

}  // namespace Components

#endif
