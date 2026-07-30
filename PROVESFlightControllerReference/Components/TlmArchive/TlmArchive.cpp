// ======================================================================
// \title  TlmArchive.cpp
// \author aychar
// \brief  cpp file for TlmArchive component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/TlmArchive/TlmArchive.hpp"

#include "Os/File.hpp"
#include "Os/FileSystem.hpp"
#include "Os/Models/FileStatusEnumAc.hpp"

namespace Components {

namespace {

constexpr const char* TLM_DIRECTORY = "//tlm";
constexpr const char* PRE_DEPLOYMENT_TLM_PATH = "//tlm/pre_deployment.tlm";
constexpr const int MAX_FAILURES = 3;
constexpr const FwSizeType MAX_FILE_SIZE = 10000;

}  // namespace

TlmArchive ::TlmArchive(const char* const compName) : TlmArchiveComponentBase(compName) {}

TlmArchive ::~TlmArchive() {}

void TlmArchive::comIn_handler(FwIndexType portNum, Fw::ComBuffer& data, U32 context) {
    (void)portNum;
    (void)context;

    if (this->m_failures.load() >= MAX_FAILURES) {
        this->log_WARNING_HI_FailureLimitReached(MAX_FAILURES);
        return;
    } else if (this->m_fileSize.load() >= MAX_FILE_SIZE) {
        this->log_WARNING_LO_SizeLimitReached(MAX_FILE_SIZE);
        return;
    } else if (this->m_antennasDeployed.load()) {
        this->log_WARNING_LO_AntennasDeployed();
        return;
    }

    {
        Os::ScopeLock lock(this->m_queueMutex);
        this->m_pendingPacket = data;
        this->m_packetPending = true;
    }
}

void TlmArchive::run_handler(FwIndexType portNum, U32 context) {
    (void)portNum;
    (void)context;

    Fw::ComBuffer packet;
    {
        Os::ScopeLock lock(this->m_queueMutex);
        if (!this->m_packetPending) {
            return;
        }
        packet = this->m_pendingPacket;
        this->m_packetPending = false;
    }

    if (!this->m_antennasDeployed.load()) {
        this->m_antennasDeployed.store(this->deploymentStateGet_out(0));
    }

    if (this->m_antennasDeployed.load()) {
        return;
    }

    if (!this->m_directoryInitialized) {
        if (Os::FileSystem::createDirectory(TLM_DIRECTORY, false) != Os::FileSystem::OP_OK) {
            this->log_WARNING_HI_FileError(Fw::LogStringArg("create_directory"));
            this->m_failures.fetch_add(1);
            return;
        }
        if (Os::FileSystem::touch(PRE_DEPLOYMENT_TLM_PATH) != Os::FileSystem::OP_OK) {
            this->log_WARNING_HI_FileError(Fw::LogStringArg("create_file"));
            this->m_failures.fetch_add(1);
            return;
        }
        this->m_directoryInitialized = true;
    }

    const FwSizeType requestedSize = packet.getSize();
    FwSizeType currentSize = this->m_fileSize.load();
    if (!this->m_fileSizeInitialized) {
        const Os::FileSystem::Status sizeStatus = Os::FileSystem::getFileSize(PRE_DEPLOYMENT_TLM_PATH, currentSize);
        if (sizeStatus != Os::FileSystem::OP_OK) {
            this->log_WARNING_HI_FileError(Fw::LogStringArg("get_file_size"));
            this->m_failures.fetch_add(1);
            return;
        }
        const U32 cachedSize =
            (currentSize > MAX_FILE_SIZE) ? static_cast<U32>(MAX_FILE_SIZE) : static_cast<U32>(currentSize);
        this->m_fileSize.store(cachedSize);
        this->m_fileSizeInitialized = true;
    }

    if ((currentSize > MAX_FILE_SIZE)) {
        this->m_failures.fetch_add(1);
        return;
    }

    this->log_ACTIVITY_LO_WriteStart();
    Os::File file;
    const Os::File::Status openStatus = file.open(PRE_DEPLOYMENT_TLM_PATH, Os::File::OPEN_APPEND);
    if (openStatus != Os::File::OP_OK) {
        this->log_WARNING_HI_FileError(Fw::LogStringArg("open_append"));
        this->m_failures.fetch_add(1);
        return;
    }

    FwSizeType writtenSize = requestedSize;
    const Os::File::Status writeStatus = file.write(packet.getBuffAddr(), writtenSize);
    if ((writeStatus != Os::File::OP_OK) || (writtenSize != requestedSize)) {
        this->log_WARNING_HI_WriteError(Os::FileStatus(static_cast<Os::FileStatus::T>(writeStatus)), requestedSize,
                                        writtenSize);
        this->m_failures.fetch_add(1);
        file.close();
        return;
    }
    file.close();

    this->m_fileSize.fetch_add(static_cast<U32>(writtenSize));
}

}  // namespace Components
