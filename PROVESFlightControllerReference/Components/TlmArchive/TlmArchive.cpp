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

    bool writeDisabled = false;
    {
        Os::ScopeLock lock(this->m_queueMutex);
        writeDisabled = this->m_failures >= MAX_FAILURES || this->m_fileSize >= MAX_FILE_SIZE;
        if (!writeDisabled && !this->m_packetPending) {
            this->m_pendingPacket = data;
            this->m_packetPending = true;
        }
    }

    if (writeDisabled) {
        this->log_WARNING_HI_ArchiveWriteDisabled(MAX_FAILURES, MAX_FILE_SIZE);
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

    if (!this->m_antennasDeployed) {
        this->m_antennasDeployed = this->deploymentStateGet_out(0);
    }

    if (this->m_antennasDeployed) {
        return;
    }

    if (!this->m_directoryInitialized &&
        Os::FileSystem::createDirectory(TLM_DIRECTORY, false) != Os::FileSystem::OP_OK) {
        this->log_WARNING_HI_ArchiveFileError(Fw::LogStringArg("create_directory"));
        {
            Os::ScopeLock lock(this->m_queueMutex);
            this->m_failures++;
        }
        return;
    }
    this->m_directoryInitialized = true;

    this->log_ACTIVITY_LO_ArchiveWriteStart();
    Os::File file;
    const Os::File::Status openStatus = file.open(PRE_DEPLOYMENT_TLM_PATH, Os::File::OPEN_APPEND);
    if (openStatus != Os::File::OP_OK) {
        this->log_WARNING_HI_ArchiveFileError(Fw::LogStringArg("open_append"));
        {
            Os::ScopeLock lock(this->m_queueMutex);
            this->m_failures++;
        }
        return;
    }

    const FwSizeType requestedSize = packet.getSize();
    FwSizeType writtenSize = requestedSize;
    const Os::File::Status writeStatus = file.write(packet.getBuffAddr(), writtenSize);
    if ((writeStatus != Os::File::OP_OK) || (writtenSize != requestedSize)) {
        this->log_WARNING_HI_ArchiveWriteError(Os::FileStatus(static_cast<Os::FileStatus::T>(writeStatus)),
                                               requestedSize, writtenSize);
        {
            Os::ScopeLock lock(this->m_queueMutex);
            this->m_failures++;
        }
    } else {
        this->log_ACTIVITY_LO_ArchiveWriteFinish();
    }
    file.close();

    // File size not initialized yet
    bool fileSizeUninitialized = false;
    {
        Os::ScopeLock lock(this->m_queueMutex);
        fileSizeUninitialized = this->m_fileSize == 0;
    }
    if (fileSizeUninitialized) {
        FwSizeType size_arg;
        const Os::FileSystem::Status status = Os::FileSystem::getFileSize(PRE_DEPLOYMENT_TLM_PATH, size_arg);
        if (status != Os::FileSystem::OP_OK || size_arg == 0) {
            this->log_WARNING_HI_ArchiveFileError(Fw::LogStringArg("file size still 0 after write"));
            return;
        }

        {
            Os::ScopeLock lock(this->m_queueMutex);
            this->m_fileSize = size_arg;
        }
        return;
    }

    {
        Os::ScopeLock lock(this->m_queueMutex);
        this->m_fileSize += requestedSize;
    }
}

}  // namespace Components
