// ======================================================================
// \title  TlmArchive.cpp
// \author aychar
// \brief  cpp file for TlmArchive component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/TlmArchive/TlmArchive.hpp"

#include <cstdio>
#include <cstring>

#include "Os/Directory.hpp"
#include "Os/FileSystem.hpp"

namespace Components {

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
        bool status = this->writeRecord();
        if (!status) {
            this->log_ACTIVITY_LO_Debug(Fw::LogStringArg("Failed to write telemetry record"));
        }
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
    // TODO: Ensure capacity & prune old if at capacity
    // TODO: Check boot status for if packet should be permanent
    // TODO: Add record to db.txt file

    if (!this->openFile()) {
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
        this->log_ACTIVITY_LO_Debug(Fw::LogStringArg("Failed to write telemetry data to file"));
        return false;
    }
    if (!this->closeFile()) {
        this->log_ACTIVITY_LO_Debug(Fw::LogStringArg("Failed to close telemetry file"));
        return false;
    }

    this->m_packetCount = 0;
    return true;
}

bool TlmArchive::openFile() {
    if (this->m_fileOpen || !this->ensureDirectories()) {
        return false;
    }

    char fileName[96U] = {};
    std::snprintf(fileName, sizeof(fileName), "//tlm/tlm_%08u.tlm", this->m_fileCount++);
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
    return Os::FileSystem::createDirectory("//tlm", false) == Os::FileSystem::OP_OK;
}

}  // namespace Components
