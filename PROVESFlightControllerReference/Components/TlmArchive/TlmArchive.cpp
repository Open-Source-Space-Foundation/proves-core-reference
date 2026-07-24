// ======================================================================
// \title  TlmArchive.cpp
// \author aychar
// \brief  cpp file for TlmArchive component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/TlmArchive/TlmArchive.hpp"

#include <cstdio>
#include <cstring>
#include <limits>

#include "Os/Directory.hpp"
#include "Os/FileSystem.hpp"

namespace Components {

namespace {

constexpr const char* TLM_DIRECTORY = "//tlm";
constexpr const char* FIRST_BOOT_DIRECTORY = "//tlm/firstboot";
constexpr const char* TLM_COUNT_PATH = "//tlm/file_count.txt";
constexpr const char* FIRST_BOOT_COUNT_PATH = "//tlm/firstboot/file_count.txt";
constexpr FwSizeType FILE_NAME_BUFFER_SIZE = 48U;
constexpr FwSizeType FILE_COUNT_BUFFER_SIZE = 12U;
constexpr U32 MAX_FILE_ID = 99999999U;

bool parseFileName(const char* fileName, U32& fileId) {
    if ((fileName == nullptr) || (std::strlen(fileName) != 16U) || (std::strncmp(fileName, "tlm_", 4U) != 0) ||
        (std::strcmp(&fileName[12], ".tlm") != 0)) {
        return false;
    }

    U32 parsedId = 0U;
    for (U32 i = 4U; i < 12U; i++) {
        if ((fileName[i] < '0') || (fileName[i] > '9')) {
            return false;
        }
        parsedId = (parsedId * 10U) + static_cast<U32>(fileName[i] - '0');
    }
    fileId = parsedId;
    return true;
}

bool parseFileCount(const char* text, U32& count) {
    if ((text == nullptr) || (*text < '0') || (*text > '9')) {
        return false;
    }

    U32 parsedCount = 0U;
    const char* cursor = text;
    while ((*cursor >= '0') && (*cursor <= '9')) {
        const U32 digit = static_cast<U32>(*cursor - '0');
        if (parsedCount > ((std::numeric_limits<U32>::max() - digit) / 10U)) {
            return false;
        }
        parsedCount = (parsedCount * 10U) + digit;
        cursor++;
    }
    if (*cursor == '\n') {
        cursor++;
    }
    if (*cursor != '\0') {
        return false;
    }
    count = parsedCount;
    return true;
}

}  // namespace

TlmArchive ::TlmArchive(const char* const compName) : TlmArchiveComponentBase(compName) {}

TlmArchive ::~TlmArchive() {}

void TlmArchive::comIn_handler(FwIndexType portNum, Fw::ComBuffer& data, U32 context) {
    (void)portNum;
    (void)context;

    Os::ScopeLock lock(this->m_mutex);
    if (!this->m_enabled) {
        return;
    }

    if (this->m_packetCount == MAX_STORED_PACKETS) {
        this->log_ACTIVITY_LO_Debug(Fw::LogStringArg("Writing record."));
        bool status = this->writeRecord();
        if (!status) {
            this->log_ACTIVITY_LO_Debug(Fw::LogStringArg("Failed to write telemetry record"));
        }
        this->log_ACTIVITY_LO_Debug(Fw::LogStringArg("Done writing record."));
    }

    if (this->m_packetCount < MAX_STORED_PACKETS) {
        this->m_packetArr[this->m_packetCount] = data;
        this->m_packetCount++;
    }
}

void TlmArchive::run_handler(FwIndexType portNum, U32 context) {
    (void)portNum;
    (void)context;

    Os::ScopeLock lock(this->m_mutex);
    if (this->m_enabled && (this->m_packetCount > 0U) && !this->writeRecord()) {
        this->log_ACTIVITY_LO_Debug(Fw::LogStringArg("Failed to write telemetry record"));
    }
}

void TlmArchive::RECORDING_STATUS_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Components::Status status) {
    {
        Os::ScopeLock lock(this->m_mutex);
        this->m_enabled = (status == Components::Status::ENABLED);
        this->tlmWrite_RecordingEnabled(this->m_enabled ? Fw::On::ON : Fw::On::OFF);
    }
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

bool TlmArchive::writeRecord() {
    if (!this->m_directoriesReady) {
        this->m_directoriesReady = this->ensureDirectories();
        if (!this->m_directoriesReady) {
            return false;
        }
    }

    const bool deployed = this->deploymentStateGet_out(0);
    const char* const directory = deployed ? TLM_DIRECTORY : FIRST_BOOT_DIRECTORY;
    const char* const countPath = deployed ? TLM_COUNT_PATH : FIRST_BOOT_COUNT_PATH;
    U32& fileCount = deployed ? this->m_regularFileCount : this->m_firstBootFileCount;
    bool& fileCountLoaded = deployed ? this->m_regularFileCountLoaded : this->m_firstBootFileCountLoaded;

    if (!fileCountLoaded) {
        if (!this->loadFileCount(directory, countPath, fileCount)) {
            this->log_ACTIVITY_LO_Debug(Fw::LogStringArg("Failed to load telemetry file count"));
            return false;
        }
        fileCountLoaded = true;
    }

    if (deployed && !this->pruneOldFiles(fileCount)) {
        return false;
    }

    char fileName[FILE_NAME_BUFFER_SIZE] = {};
    if (!this->formatFileName(directory, fileCount, fileName, sizeof(fileName)) || !this->openFile(fileName)) {
        return false;
    }

    FwSizeType fileDataSize = 0;
    std::memset(this->m_fileData, 0, sizeof(this->m_fileData));

    for (U32 i = 0; i < this->m_packetCount; i++) {
        const FwSizeType pktSize = this->m_packetArr[i].getSize();
        std::memcpy(&this->m_fileData[fileDataSize], this->m_packetArr[i].getBuffAddr(),
                    static_cast<std::size_t>(pktSize));
        fileDataSize += pktSize;
    }

    if (!this->writeToFile(fileDataSize)) {
        (void)Os::FileSystem::removeFile(fileName);
        this->log_ACTIVITY_LO_Debug(Fw::LogStringArg("Failed to write telemetry data to file"));
        return false;
    }
    if (!this->closeFile()) {
        (void)Os::FileSystem::removeFile(fileName);
        this->log_ACTIVITY_LO_Debug(Fw::LogStringArg("Failed to close telemetry file"));
        return false;
    }

    this->m_packetCount = 0;
    fileCount++;
    if (!this->writeFileCount(countPath, fileCount)) {
        this->log_ACTIVITY_LO_Debug(Fw::LogStringArg("Failed to update telemetry file count"));
    }
    return true;
}

bool TlmArchive::openFile(const char* fileName) {
    if (this->m_fileOpen) {
        return false;
    }

    Os::FileInterface::Status status = this->m_file.open(fileName, Os::File::OPEN_CREATE, Os::File::NO_OVERWRITE);
    if (status != Os::File::OP_OK) {
        this->log_ACTIVITY_LO_Debug(Fw::LogStringArg("Failed to open telemetry file"));
        return false;
    }

    this->m_fileOpen = true;
    return true;
}

bool TlmArchive::closeFile() {
    if (!this->m_fileOpen) {
        return false;
    }

    this->m_file.close();
    this->m_fileOpen = false;
    return true;
}

bool TlmArchive::writeToFile(FwSizeType fileDataSize) {
    if (!this->m_fileOpen) {
        return false;
    }

    const FwSizeType expectedSize = fileDataSize;
    if ((this->m_file.write(this->m_fileData, fileDataSize) != Os::File::OP_OK) || (fileDataSize != expectedSize)) {
        this->closeFile();
        this->log_ACTIVITY_LO_Debug(Fw::LogStringArg("Failed to write to telemetry file"));
        return false;
    }

    return true;
}

bool TlmArchive::ensureDirectories() {
    return (Os::FileSystem::createDirectory(TLM_DIRECTORY, false) == Os::FileSystem::OP_OK) &&
           (Os::FileSystem::createDirectory(FIRST_BOOT_DIRECTORY, false) == Os::FileSystem::OP_OK);
}

bool TlmArchive::loadFileCount(const char* directory, const char* countPath, U32& count) {
    if (!this->readFileCount(countPath, count)) {
        U32 retainedCount = 0U;
        U32 lowestId = 0U;
        while (true) {
            if (!this->scanFileCount(directory, count, retainedCount, lowestId)) {
                return false;
            }
            if ((std::strcmp(directory, TLM_DIRECTORY) != 0) || (retainedCount <= 100U)) {
                break;
            }
            if (!this->removeFileRange(lowestId)) {
                return false;
            }
        }
        if (!this->writeFileCount(countPath, count)) {
            this->log_ACTIVITY_LO_Debug(Fw::LogStringArg("Failed to initialize telemetry file count"));
        }
        return true;
    }

    const U32 savedCount = count;
    char fileName[FILE_NAME_BUFFER_SIZE] = {};
    while ((count <= MAX_FILE_ID) && this->formatFileName(directory, count, fileName, sizeof(fileName)) &&
           Os::FileSystem::exists(fileName)) {
        count++;
    }

    if ((count != savedCount) && !this->writeFileCount(countPath, count)) {
        this->log_ACTIVITY_LO_Debug(Fw::LogStringArg("Failed to repair telemetry file count"));
    }
    return true;
}

bool TlmArchive::readFileCount(const char* countPath, U32& count) {
    Os::File countFile;
    if (countFile.open(countPath, Os::File::OPEN_READ) != Os::File::OP_OK) {
        return false;
    }

    FwSizeType fileSize = 0U;
    if ((countFile.size(fileSize) != Os::File::OP_OK) || (fileSize == 0U) || (fileSize >= FILE_COUNT_BUFFER_SIZE)) {
        countFile.close();
        return false;
    }

    char buffer[FILE_COUNT_BUFFER_SIZE] = {};
    FwSizeType readSize = fileSize;
    const Os::File::Status status = countFile.read(reinterpret_cast<U8*>(buffer), readSize);
    countFile.close();
    return (status == Os::File::OP_OK) && (readSize == fileSize) && parseFileCount(buffer, count);
}

bool TlmArchive::writeFileCount(const char* countPath, U32 count) {
    char buffer[FILE_COUNT_BUFFER_SIZE] = {};
    const int countSize = std::snprintf(buffer, sizeof(buffer), "%u\n", count);
    if ((countSize <= 0) || (static_cast<FwSizeType>(countSize) >= sizeof(buffer))) {
        return false;
    }

    Os::File countFile;
    if (countFile.open(countPath, Os::File::OPEN_CREATE, Os::File::OVERWRITE) != Os::File::OP_OK) {
        return false;
    }

    FwSizeType writeSize = static_cast<FwSizeType>(std::strlen(buffer));
    const FwSizeType expectedSize = writeSize;
    const Os::File::Status status = countFile.write(reinterpret_cast<const U8*>(buffer), writeSize);
    countFile.close();
    return (status == Os::File::OP_OK) && (writeSize == expectedSize);
}

bool TlmArchive::scanFileCount(const char* directory, U32& count, U32& retainedCount, U32& lowestId) {
    Os::Directory archiveDirectory;
    if (archiveDirectory.open(directory, Os::Directory::READ) != Os::Directory::OP_OK) {
        return false;
    }

    bool foundFile = false;
    U32 highestId = 0U;
    retainedCount = 0U;
    char fileName[FILE_NAME_BUFFER_SIZE] = {};
    Os::Directory::Status status = Os::Directory::OP_OK;
    while ((status = archiveDirectory.read(fileName, sizeof(fileName))) == Os::Directory::OP_OK) {
        U32 fileId = 0U;
        if (parseFileName(fileName, fileId)) {
            if (!foundFile || (fileId < lowestId)) {
                lowestId = fileId;
            }
            if (!foundFile || (fileId > highestId)) {
                highestId = fileId;
            }
            foundFile = true;
            retainedCount++;
        }
    }
    archiveDirectory.close();

    if (status != Os::Directory::NO_MORE_FILES) {
        return false;
    }
    count = foundFile ? highestId + 1U : 0U;
    return true;
}

bool TlmArchive::formatFileName(const char* directory, U32 fileId, char* fileName, FwSizeType fileNameSize) {
    if (fileId > MAX_FILE_ID) {
        return false;
    }
    const int size =
        std::snprintf(fileName, static_cast<std::size_t>(fileNameSize), "%s/tlm_%08u.tlm", directory, fileId);
    return (size > 0) && (static_cast<FwSizeType>(size) < fileNameSize);
}

bool TlmArchive::pruneOldFiles(U32 nextFileId) {
    if ((nextFileId < 100U) || ((nextFileId % 10U) != 0U)) {
        return true;
    }

    return this->removeFileRange(nextFileId - 100U);
}

bool TlmArchive::removeFileRange(U32 firstId) {
    char fileName[FILE_NAME_BUFFER_SIZE] = {};
    for (U32 id = firstId; id < (firstId + 10U); id++) {
        if (!this->formatFileName(TLM_DIRECTORY, id, fileName, sizeof(fileName))) {
            return false;
        }
        const Os::FileSystem::Status status = Os::FileSystem::removeFile(fileName);
        if ((status != Os::FileSystem::OP_OK) && (status != Os::FileSystem::DOESNT_EXIST)) {
            this->log_ACTIVITY_LO_Debug(Fw::LogStringArg("Failed to prune old telemetry file"));
            return false;
        }
    }
    return true;
}

}  // namespace Components
