#include "PROVESFlightControllerReference/Components/TelemetryArchive/TelemetryArchive.hpp"

#include <cstdio>
#include <cstring>

#include "Fw/Types/Assert.hpp"
#include "Os/Directory.hpp"
#include "Os/FileSystem.hpp"
#include <zephyr/kernel.h>

namespace Components {
namespace {

constexpr const char* ROOT_DIR = "/tlm";
constexpr const char* DEPLOYMENT_DIR = "/tlm/deployment";
constexpr const char* ROLLING_DIR = "/tlm/rolling";
constexpr const char* ANTENNA_STATE_FILE = "/antenna/antenna_deployer.bin";
constexpr U8 FILE_HEADER[] = {'T', 'L', 'M', 'A', 'R', 'C', 'H', '1', 0, 1, 0, 0};
constexpr U32 RECORD_OVERHEAD = sizeof(U16) + sizeof(U32) + sizeof(U32);

}  // namespace

TelemetryArchive::TelemetryArchive(const char* const compName) : TelemetryArchiveComponentBase(compName) {}

TelemetryArchive::~TelemetryArchive() {
    this->closeSegment(false);
}

void TelemetryArchive::init(FwEnumStoreType instance) {
    TelemetryArchiveComponentBase::init(instance);
    this->m_deploymentMode = !this->readDeploymentState();
    if (!this->ensureDirectories()) {
        this->pause("directory initialization failed");
    }
    U32 ignored = 0;
    const char* directory = this->m_deploymentMode ? DEPLOYMENT_DIR : ROLLING_DIR;
    const char* prefix = this->m_deploymentMode ? "deployment_" : "rolling_";
    (void)this->scanDirectory(directory, prefix, this->m_segmentSequence, &ignored);
    this->publishState();
}

void TelemetryArchive::comIn_handler(FwIndexType portNum, Fw::ComBuffer& data, U32 context) {
    (void)portNum;
    (void)context;
    Os::ScopeLock lock(this->m_mutex);
    if (!this->m_enabled || this->m_paused || !this->writeRecord(data)) {
        this->m_recordsDropped++;
        this->tlmWrite_RecordsDropped(this->m_recordsDropped);
    }
}

void TelemetryArchive::deploymentComplete_handler(FwIndexType portNum) {
    (void)portNum;
    Os::ScopeLock lock(this->m_mutex);
    if (!this->m_deploymentMode) {
        return;
    }
    char protectedFile[PATH_SIZE] = {};
    (void)std::snprintf(protectedFile, sizeof(protectedFile), "%s", this->m_fileName);
    this->closeSegment();
    this->m_deploymentMode = false;
    this->m_paused = false;
    U32 ignored = 0;
    (void)this->scanDirectory(ROLLING_DIR, "rolling_", this->m_segmentSequence, &ignored);
    if (protectedFile[0] != '\0') {
        this->log_ACTIVITY_HI_DeploymentCaptureComplete(Fw::LogStringArg(protectedFile));
    }
    this->publishState();
}

void TelemetryArchive::ENABLE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Os::ScopeLock lock(this->m_mutex);
    this->m_enabled = true;
    this->m_paused = false;
    this->publishState();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void TelemetryArchive::DISABLE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Os::ScopeLock lock(this->m_mutex);
    this->m_enabled = false;
    this->closeSegment();
    this->publishState();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void TelemetryArchive::CLOSE_SEGMENT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Os::ScopeLock lock(this->m_mutex);
    this->closeSegment();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void TelemetryArchive::PURGE_ROLLING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Os::ScopeLock lock(this->m_mutex);
    if (!this->m_deploymentMode) {
        this->closeSegment();
    }
    U32 next = 0;
    U32 oldest = 0;
    U32 usage = this->scanDirectory(ROLLING_DIR, "rolling_", next, &oldest);
    while (usage > 0U && this->pruneOldestRolling(usage)) {
    }
    this->m_paused = false;
    this->publishState();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void TelemetryArchive::GET_STATUS_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Os::ScopeLock lock(this->m_mutex);
    this->log_ACTIVITY_LO_ArchiveStatus(
        this->m_deploymentMode ? TelemetryArchiveMode::DEPLOYMENT : TelemetryArchiveMode::ROLLING, this->m_enabled,
        this->m_paused, this->m_recordsStored, this->m_bytesStored);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

bool TelemetryArchive::readDeploymentState() const {
    Os::File file;
    if (file.open(ANTENNA_STATE_FILE, Os::File::OPEN_READ) != Os::File::OP_OK) {
        return false;
    }
    U8 value = 0;
    FwSizeType size = sizeof(value);
    const Os::File::Status status = file.read(&value, size);
    file.close();
    return status == Os::File::OP_OK && size == sizeof(value) && value == 1U;
}

bool TelemetryArchive::ensureDirectories() {
    return Os::FileSystem::createDirectory(ROOT_DIR, false) == Os::FileSystem::OP_OK &&
           Os::FileSystem::createDirectory(DEPLOYMENT_DIR, false) == Os::FileSystem::OP_OK &&
           Os::FileSystem::createDirectory(ROLLING_DIR, false) == Os::FileSystem::OP_OK;
}

bool TelemetryArchive::openSegment() {
    if (!this->ensureCapacity()) {
        return false;
    }
    const char* directory = this->m_deploymentMode ? DEPLOYMENT_DIR : ROLLING_DIR;
    const char* prefix = this->m_deploymentMode ? "deployment" : "rolling";
    const int length = std::snprintf(this->m_fileName, sizeof(this->m_fileName), "%s/%s_%08u.tlm", directory, prefix,
                                     this->m_segmentSequence++);
    if (length <= 0 || static_cast<FwSizeType>(length) >= sizeof(this->m_fileName) ||
        this->m_file.open(this->m_fileName, Os::File::OPEN_CREATE, Os::File::NO_OVERWRITE) != Os::File::OP_OK) {
        this->pause("segment open failed");
        return false;
    }
    FwSizeType size = sizeof(FILE_HEADER);
    if (this->m_file.write(FILE_HEADER, size) != Os::File::OP_OK || size != sizeof(FILE_HEADER)) {
        this->m_file.close();
        this->pause("segment header write failed");
        return false;
    }
    this->m_fileOpen = true;
    this->m_segmentBytes = sizeof(FILE_HEADER);
    this->m_recordsSinceSync = 0;
    this->log_ACTIVITY_LO_SegmentOpened(Fw::LogStringArg(this->m_fileName), this->m_deploymentMode);
    return true;
}

void TelemetryArchive::closeSegment(bool emitEvent) {
    if (!this->m_fileOpen) {
        return;
    }
    (void)this->m_file.flush();
    this->m_file.close();
    this->m_fileOpen = false;
    this->m_recordsSinceSync = 0;
    if (emitEvent) {
        this->log_ACTIVITY_HI_SegmentClosed(Fw::LogStringArg(this->m_fileName));
    }
}

bool TelemetryArchive::writeRecord(const Fw::ComBuffer& data) {
    const FwSizeType packetSize = data.getSize();
    if (packetSize > FW_COM_BUFFER_MAX_SIZE) {
        return false;
    }
    const U32 recordSize = static_cast<U32>(RECORD_OVERHEAD + packetSize);
    if (this->m_fileOpen && this->m_segmentBytes + recordSize > SEGMENT_SIZE) {
        this->closeSegment();
    }
    if (!this->m_fileOpen && !this->openSegment()) {
        return false;
    }

    U8 record[RECORD_OVERHEAD + FW_COM_BUFFER_MAX_SIZE] = {};
    this->putU16(record, static_cast<U16>(packetSize));
    this->putU32(&record[sizeof(U16)], this->m_recordSequence++);
    std::memcpy(&record[sizeof(U16) + sizeof(U32)], data.getBuffAddr(), packetSize);
    const U32 checksum = this->crc32(&record[sizeof(U16)], sizeof(U32) + packetSize);
    this->putU32(&record[sizeof(U16) + sizeof(U32) + packetSize], checksum);

    const U32 startCycles = k_cycle_get_32();
    FwSizeType writeSize = recordSize;
    const Os::File::Status status = this->m_file.write(record, writeSize);
    bool success = status == Os::File::OP_OK && writeSize == recordSize;
    this->m_recordsSinceSync++;
    if (success && (this->m_deploymentMode || this->m_recordsSinceSync >= NORMAL_SYNC_RECORDS)) {
        success = this->m_file.flush() == Os::File::OP_OK;
        this->m_recordsSinceSync = 0;
    }
    const U32 durationUs = k_cyc_to_us_floor32(k_cycle_get_32() - startCycles);
    this->tlmWrite_LastWriteDurationUs(durationUs);
    if (durationUs > this->m_maxWriteDurationUs) {
        this->m_maxWriteDurationUs = durationUs;
        this->tlmWrite_MaxWriteDurationUs(this->m_maxWriteDurationUs);
    }
    if (!success) {
        this->closeSegment(false);
        this->pause("segment write or sync failed");
        return false;
    }
    this->m_segmentBytes += recordSize;
    this->m_recordsStored++;
    this->m_bytesStored += recordSize;
    this->tlmWrite_RecordsStored(this->m_recordsStored);
    this->tlmWrite_BytesStored(this->m_bytesStored);
    return true;
}

bool TelemetryArchive::ensureCapacity() {
    FwSizeType total = 0;
    FwSizeType free = 0;
    if (Os::FileSystem::getFreeSpace("/prmDb.dat", total, free) != Os::FileSystem::OP_OK) {
        this->pause("free space unavailable");
        return false;
    }
    this->tlmWrite_FreeSpace(free);
    const FwSizeType reserve = (total / 5U > MIN_FREE_BYTES) ? total / 5U : MIN_FREE_BYTES;
    U32 next = 0;
    U32 oldest = 0;
    const char* directory = this->m_deploymentMode ? DEPLOYMENT_DIR : ROLLING_DIR;
    const char* prefix = this->m_deploymentMode ? "deployment_" : "rolling_";
    U32 usage = this->scanDirectory(directory, prefix, next, &oldest);
    const U32 budget = this->m_deploymentMode ? DEPLOYMENT_BUDGET : ROLLING_BUDGET;
    if (this->m_deploymentMode) {
        if (usage + SEGMENT_SIZE > budget || free < reserve + SEGMENT_SIZE) {
            this->pause("deployment archive full");
            return false;
        }
        return true;
    }
    while ((usage + SEGMENT_SIZE > budget || free < reserve + SEGMENT_SIZE) && usage > 0U) {
        if (!this->pruneOldestRolling(usage)) {
            this->pause("rolling prune failed");
            return false;
        }
        if (Os::FileSystem::getFreeSpace("/prmDb.dat", total, free) != Os::FileSystem::OP_OK) {
            return false;
        }
    }
    if (free < reserve + SEGMENT_SIZE) {
        this->pause("filesystem reserve reached");
        return false;
    }
    return true;
}

U32 TelemetryArchive::scanDirectory(const char* directory, const char* prefix, U32& nextSequence, U32* oldestSequence) {
    Os::Directory dir;
    nextSequence = 0;
    U32 oldest = 0xFFFFFFFFU;
    U32 usage = 0;
    if (dir.open(directory, Os::Directory::READ) != Os::Directory::OP_OK) {
        return 0;
    }
    char name[48] = {};
    while (dir.read(name, sizeof(name)) == Os::Directory::OP_OK) {
        U32 sequence = 0;
        if (std::sscanf(name, (std::strcmp(prefix, "deployment_") == 0) ? "deployment_%u.tlm" : "rolling_%u.tlm",
                        &sequence) != 1) {
            continue;
        }
        nextSequence = (sequence >= nextSequence) ? sequence + 1U : nextSequence;
        oldest = (sequence < oldest) ? sequence : oldest;
        char path[PATH_SIZE] = {};
        (void)std::snprintf(path, sizeof(path), "%s/%s", directory, name);
        Os::File file;
        FwSizeType size = 0;
        if (file.open(path, Os::File::OPEN_READ) == Os::File::OP_OK) {
            if (file.size(size) == Os::File::OP_OK) {
                usage += static_cast<U32>(size);
            }
            file.close();
        }
    }
    dir.close();
    if (oldestSequence != nullptr) {
        *oldestSequence = oldest;
    }
    return usage;
}

bool TelemetryArchive::pruneOldestRolling(U32& usage) {
    U32 next = 0;
    U32 oldest = 0;
    (void)this->scanDirectory(ROLLING_DIR, "rolling_", next, &oldest);
    if (oldest == 0xFFFFFFFFU) {
        usage = 0;
        return false;
    }
    char path[PATH_SIZE] = {};
    (void)std::snprintf(path, sizeof(path), "%s/rolling_%08u.tlm", ROLLING_DIR, oldest);
    Os::File file;
    FwSizeType size = 0;
    if (file.open(path, Os::File::OPEN_READ) == Os::File::OP_OK) {
        (void)file.size(size);
        file.close();
    }
    if (Os::FileSystem::removeFile(path) != Os::FileSystem::OP_OK) {
        return false;
    }
    const U32 removed = static_cast<U32>(size);
    usage = (removed <= usage) ? usage - removed : 0U;
    this->m_rollingBytesPruned += removed;
    this->tlmWrite_RollingBytesPruned(this->m_rollingBytesPruned);
    return true;
}

void TelemetryArchive::pause(const char* reason) {
    this->m_paused = true;
    this->log_WARNING_HI_ArchivePaused(Fw::LogStringArg(reason));
    this->publishState();
}

void TelemetryArchive::publishState() {
    this->tlmWrite_Mode(this->m_deploymentMode ? TelemetryArchiveMode::DEPLOYMENT : TelemetryArchiveMode::ROLLING);
    this->tlmWrite_Enabled(this->m_enabled);
    this->tlmWrite_Paused(this->m_paused);
}

U32 TelemetryArchive::crc32(const U8* data, FwSizeType size) {
    U32 crc = 0xFFFFFFFFU;
    for (FwSizeType i = 0; i < size; i++) {
        crc ^= data[i];
        for (U8 bit = 0; bit < 8U; bit++) {
            crc = (crc >> 1U) ^ ((crc & 1U) ? 0xEDB88320U : 0U);
        }
    }
    return ~crc;
}

void TelemetryArchive::putU16(U8* destination, U16 value) {
    destination[0] = static_cast<U8>(value >> 8U);
    destination[1] = static_cast<U8>(value);
}

void TelemetryArchive::putU32(U8* destination, U32 value) {
    destination[0] = static_cast<U8>(value >> 24U);
    destination[1] = static_cast<U8>(value >> 16U);
    destination[2] = static_cast<U8>(value >> 8U);
    destination[3] = static_cast<U8>(value);
}

}  // namespace Components
