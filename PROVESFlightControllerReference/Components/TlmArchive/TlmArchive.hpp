// ======================================================================
// \title  TlmArchive.hpp
// \author aychar
// \brief  hpp file for TlmArchive component implementation class
// ======================================================================

#ifndef Components_TlmArchive_HPP
#define Components_TlmArchive_HPP

#include "Os/File.hpp"
#include "Os/Mutex.hpp"
#include "PROVESFlightControllerReference/Components/TlmArchive/TlmArchiveComponentAc.hpp"

namespace Components {

class TlmArchive final : public TlmArchiveComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct TlmArchive object
    TlmArchive(const char* const compName  //!< The component name
    );

    //! Destroy TlmArchive object
    ~TlmArchive();

  private:
    static constexpr U32 MAX_STORED_PACKETS = 32;
    static constexpr FwSizeType MAX_FILE_SIZE = 233 * MAX_STORED_PACKETS;

    void comIn_handler(FwIndexType portNum, Fw::ComBuffer& data, U32 context) override;
    void run_handler(FwIndexType portNum, U32 context) override;
    void RECORDING_STATUS_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Components::Status status) override;

    bool writeRecord();
    bool openFile(const char* fileName);
    bool closeFile();
    bool writeToFile(FwSizeType fileDataSize);
    bool ensureDirectories();
    bool loadFileCount(const char* directory, const char* countPath, U32& count);
    bool readFileCount(const char* countPath, U32& count);
    bool writeFileCount(const char* countPath, U32 count);
    bool scanFileCount(const char* directory, U32& count, U32& retainedCount, U32& lowestId);
    bool formatFileName(const char* directory, U32 fileId, char* fileName, FwSizeType fileNameSize);
    bool pruneOldFiles(U32 nextFileId);
    bool removeFileRange(U32 firstId);

    Os::Mutex m_mutex;
    Os::File m_file;
    bool m_fileOpen = false;
    bool m_directoriesReady = false;
    U8 m_fileData[MAX_FILE_SIZE] = {};

    bool m_enabled = true;

    U32 m_packetCount = 0;
    U32 m_regularFileCount = 0;
    U32 m_firstBootFileCount = 0;
    bool m_regularFileCountLoaded = false;
    bool m_firstBootFileCountLoaded = false;

    Fw::ComBuffer m_packetArr[MAX_STORED_PACKETS] = {};
};

}  // namespace Components

#endif
