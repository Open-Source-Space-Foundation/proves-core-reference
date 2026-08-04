// ======================================================================
// \title  MosaicManager.cpp
// \brief  cpp file for MosaicManager component implementation class
//         Receives MOSAIC gamma ray detector data over UART and stores
//         it on disk under /mosaic for later downlink
// ======================================================================

#include "PROVESFlightControllerReference/Components/MosaicManager/MosaicManager.hpp"

#include <cstdlib>
#include <cstring>

#include "Fw/Types/FileNameString.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

MosaicManager ::MosaicManager(const char* const compName) : MosaicManagerComponentBase(compName) {}

MosaicManager ::~MosaicManager() {}

void MosaicManager ::init(FwEnumStoreType instance) {
    MosaicManagerComponentBase::init(instance);

    // Create the sample directory if it does not exist
    const Os::FileSystem::Status dirStatus = Os::FileSystem::createDirectory(SAMPLE_DIR, false);
    if (dirStatus != Os::FileSystem::OP_OK) {
        const Fw::LogStringArg logFilePath(SAMPLE_DIR);
        const Fw::LogStringArg logOperation("create_directory");
        this->log_WARNING_HI_FileOperationError(logFilePath, logOperation, static_cast<U32>(dirStatus));
    }
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void MosaicManager ::dataIn_handler(FwIndexType portNum, Fw::Buffer& buffer, const Drv::ByteStreamStatus& status) {
    if (status != Drv::ByteStreamStatus::OP_OK) {
        this->log_WARNING_LO_UartReceiveError();
        this->bufferReturn_out(0, buffer);
        return;
    }

    const U8* data = buffer.getData();
    const U32 size = static_cast<U32>(buffer.getSize());

    for (U32 i = 0; i < size; i++) {
        const U8 byte = data[i];
        if (byte == '\n' || byte == '\r') {
            if (m_lineLength > 0) {
                this->processLine();
                m_lineLength = 0;
            }
        } else if (m_lineLength < MAX_LINE_LENGTH - 1) {
            m_lineBuffer[m_lineLength++] = byte;
        } else {
            // Line too long for the protocol - discard and resync on next newline
            m_lineLength = 0;
            m_parseErrors++;
            this->log_WARNING_LO_LineParseError();
            this->tlmWrite_ParseErrors(m_parseErrors);
        }
    }

    // Buffer is owned by the UART driver's buffer manager - return it
    this->bufferReturn_out(0, buffer);
}

void MosaicManager ::run_handler(FwIndexType portNum, U32 context) {
    // Flush a partially filled file that has been sitting too long
    if (m_fileOpen && ((m_samplesInFile + m_bufferedSamples) > 0)) {
        const U32 now = this->getTime().getSeconds();
        if ((now - m_fileStartSeconds) >= FLUSH_TIMEOUT_SECONDS) {
            this->closeFile();
        }
    }

    this->tlmWrite_Recording(m_recording);
    this->tlmWrite_SamplesRecorded(m_samplesRecorded);
    this->tlmWrite_FilesWritten(m_filesWritten);
    this->tlmWrite_ParseErrors(m_parseErrors);
    this->tlmWrite_FilesystemErrors(m_filesystemErrors);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void MosaicManager ::START_RECORDING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 fileCount) {
    const U32 requestedFileCount = fileCount;
    m_filesystemErrors = 0;
    this->tlmWrite_FilesystemErrors(m_filesystemErrors);

    Fw::ParamValid paramValid;
    const U32 maxFileCount = this->paramGet_MAX_FILE_COUNT(paramValid);
    FW_ASSERT((paramValid == Fw::ParamValid::VALID) || (paramValid == Fw::ParamValid::DEFAULT));

    const U32 availableFileCount = this->countRemainingFileSlots(maxFileCount);

    if (fileCount > availableFileCount) {
        this->log_WARNING_HI_RecordingFileCountLimited(fileCount, availableFileCount);
        fileCount = availableFileCount;
    }

    m_filesRemainingToRecord = fileCount;
    m_recording = (m_filesRemainingToRecord > 0);
    this->tlmWrite_Recording(m_recording);
    if (m_recording) {
        this->log_ACTIVITY_HI_RecordingStarted();
    } else if ((requestedFileCount > 0) && (availableFileCount == 0)) {
        this->log_WARNING_HI_MaxFilesReached(maxFileCount);
    }
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void MosaicManager ::STOP_RECORDING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    m_recording = false;
    m_filesRemainingToRecord = 0;
    if (m_fileOpen && ((m_samplesInFile + m_bufferedSamples) > 0)) {
        this->closeFile();
    }
    this->tlmWrite_Recording(m_recording);
    this->log_ACTIVITY_HI_RecordingStopped();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void MosaicManager ::FLUSH_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (m_fileOpen && ((m_samplesInFile + m_bufferedSamples) > 0)) {
        this->closeFile();
    }
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void MosaicManager ::CLEAR_DIRECTORY_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    const bool wasRecording = m_recording;
    m_recording = false;
    m_filesRemainingToRecord = 0;
    if (m_fileOpen) {
        this->closeFile();
    }
    this->tlmWrite_Recording(m_recording);
    if (wasRecording) {
        this->log_ACTIVITY_HI_RecordingStopped();
    }

    Fw::ParamValid paramValid;
    const U32 maxFileCount = this->paramGet_MAX_FILE_COUNT(paramValid);
    FW_ASSERT((paramValid == Fw::ParamValid::VALID) || (paramValid == Fw::ParamValid::DEFAULT));

    U32 filesRemoved = 0;
    bool success = true;
    const U32 filesToCheck = (m_nextFileIndex > maxFileCount) ? m_nextFileIndex : maxFileCount;
    for (U32 index = 0; index < filesToCheck; index++) {
        Fw::FileNameString path;
        path.format("%s/gamma_%06u.dat", SAMPLE_DIR, index);
        if (Os::FileSystem::getPathType(path.toChar()) != Os::FileSystem::FILE) {
            continue;
        }

        const Os::FileSystem::Status status = Os::FileSystem::removeFile(path.toChar());
        if (status == Os::FileSystem::OP_OK) {
            filesRemoved++;
        } else {
            const Fw::LogStringArg logFilePath(path.toChar());
            const Fw::LogStringArg logOperation("remove");
            this->log_WARNING_HI_FileOperationError(logFilePath, logOperation, static_cast<U32>(status));
            this->recordFilesystemError();
            success = false;
        }
    }

    if (success && (Os::FileSystem::getPathType(FILE_LIMIT_MARKER) == Os::FileSystem::FILE)) {
        const Os::FileSystem::Status status = Os::FileSystem::removeFile(FILE_LIMIT_MARKER);
        if (status != Os::FileSystem::OP_OK) {
            const Fw::LogStringArg logFilePath(FILE_LIMIT_MARKER);
            const Fw::LogStringArg logOperation("remove");
            this->log_WARNING_HI_FileOperationError(logFilePath, logOperation, static_cast<U32>(status));
            this->recordFilesystemError();
            success = false;
        }
    }

    if (success) {
        m_currentFileIndex = 0;
        m_nextFileIndex = 0;
        m_fileIndexInitialized = true;
        m_fileLimitReached = false;
        this->log_ACTIVITY_HI_DirectoryCleared(filesRemoved);
    }
    this->cmdResponse_out(opCode, cmdSeq, success ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

// ----------------------------------------------------------------------
// Helper methods
// ----------------------------------------------------------------------

void MosaicManager ::processLine() {
    // Expected line format from the MOSAIC firmware: "ADC=<raw>,MV=<millivolts>"
    m_lineBuffer[m_lineLength] = '\0';
    const char* line = reinterpret_cast<const char*>(m_lineBuffer);

    bool parsed = false;
    U32 adc = 0;
    U32 millivolts = 0;
    if (strncmp(line, "ADC=", 4) == 0) {
        char* end = nullptr;
        adc = static_cast<U32>(strtoul(&line[4], &end, 10));
        if ((end != &line[4]) && (strncmp(end, ",MV=", 4) == 0)) {
            const char* mvStart = end + 4;
            millivolts = static_cast<U32>(strtoul(mvStart, &end, 10));
            parsed = (end != mvStart) && (*end == '\0') && (adc <= 0xFFFF) && (millivolts <= 0xFFFF);
        }
    }

    if (!parsed) {
        m_parseErrors++;
        this->log_WARNING_LO_LineParseError();
        this->tlmWrite_ParseErrors(m_parseErrors);
        return;
    }

    this->tlmWrite_LatestAdc(static_cast<U16>(adc));
    this->tlmWrite_LatestMillivolts(static_cast<U16>(millivolts));

    if (m_recording) {
        this->recordSample(static_cast<U16>(adc), static_cast<U16>(millivolts));
    }
}

void MosaicManager ::recordSample(U16 adc, U16 millivolts) {
    if (!this->ensureFileOpen()) {
        return;
    }

    Fw::ParamValid paramValid;
    U32 samplesPerFile = this->paramGet_SAMPLES_PER_FILE(paramValid);
    FW_ASSERT((paramValid == Fw::ParamValid::VALID) || (paramValid == Fw::ParamValid::DEFAULT));
    if (samplesPerFile == 0) {
        samplesPerFile = 1;
    }

    U32 samplesPerWrite = this->paramGet_SAMPLES_PER_WRITE(paramValid);
    FW_ASSERT((paramValid == Fw::ParamValid::VALID) || (paramValid == Fw::ParamValid::DEFAULT));
    if (samplesPerWrite == 0) {
        samplesPerWrite = 1;
    }

    // If SAMPLES_PER_FILE was reduced while this file was open, finish the
    // current file before placing another sample in it.
    if ((m_samplesInFile + m_bufferedSamples) >= samplesPerFile) {
        this->closeFile();
        if (!m_recording || !this->ensureFileOpen()) {
            return;
        }
    }

    const U32 seconds = this->getTime().getSeconds();
    const FwSizeType offset = m_bufferedSamples * RECORD_SIZE;
    std::memcpy(&m_writeBuffer[offset], &seconds, sizeof(seconds));
    std::memcpy(&m_writeBuffer[offset + sizeof(seconds)], &adc, sizeof(adc));
    std::memcpy(&m_writeBuffer[offset + sizeof(seconds) + sizeof(adc)], &millivolts, sizeof(millivolts));

    if ((m_samplesInFile + m_bufferedSamples) == 0) {
        m_fileStartSeconds = seconds;
    }
    m_bufferedSamples++;

    // The per-file limit takes precedence so the last, possibly partial, batch
    // is written and the file is closed as soon as its configured size is met.
    if ((m_samplesInFile + m_bufferedSamples) >= samplesPerFile) {
        this->closeFile();
        return;
    }

    if ((m_bufferedSamples >= samplesPerWrite) && !this->writeBufferedSamples()) {
        // A short write leaves a torn batch on the end of the file. Abandon it
        // rather than advertising the file as complete and downlinkable.
        this->closeFile(false);
    }
}

bool MosaicManager ::writeBufferedSamples() {
    if (m_bufferedSamples == 0) {
        return true;
    }

    const U32 samplesToWrite = m_bufferedSamples;
    const FwSizeType expectedSize = samplesToWrite * RECORD_SIZE;
    FwSizeType writeSize = expectedSize;
    const Os::File::Status status = m_file.write(m_writeBuffer, writeSize, Os::File::WaitType::WAIT);
    if ((status != Os::File::OP_OK) || (writeSize != expectedSize)) {
        Fw::FileNameString path;
        path.format("%s/gamma_%06u.dat", SAMPLE_DIR, m_currentFileIndex);
        const Fw::LogStringArg logFilePath(path.toChar());
        const Fw::LogStringArg logOperation("write");
        this->log_WARNING_HI_FileOperationError(logFilePath, logOperation, static_cast<U32>(status));
        this->recordFilesystemError();
        m_bufferedSamples = 0;
        return false;
    }

    m_bufferedSamples = 0;
    m_samplesInFile += samplesToWrite;
    m_samplesRecorded += samplesToWrite;
    this->tlmWrite_SamplesRecorded(m_samplesRecorded);
    return true;
}

bool MosaicManager ::ensureFileOpen() {
    if (m_fileOpen) {
        return true;
    }

    Fw::ParamValid paramValid;
    const U32 maxFileCount = this->paramGet_MAX_FILE_COUNT(paramValid);
    FW_ASSERT((paramValid == Fw::ParamValid::VALID) || (paramValid == Fw::ParamValid::DEFAULT));

    this->initializeFileIndex(maxFileCount);

    // The Zephyr Os::File delegate truncates on OPEN_CREATE regardless of the NO_OVERWRITE flag
    // (its handling of `overwrite` is unimplemented). Move forward from the monotonic cursor and
    // skip occupied names, but never wrap or reuse a lower index. CLEAR_DIRECTORY is the only
    // operation that resets the cursor after the configured limit is reached.
    Fw::FileNameString path;
    bool slotFound = false;
    while (!m_fileLimitReached && (m_nextFileIndex < maxFileCount)) {
        const U32 index = m_nextFileIndex;
        m_nextFileIndex++;
        path.format("%s/gamma_%06u.dat", SAMPLE_DIR, index);
        if (Os::FileSystem::getPathType(path.toChar()) == Os::FileSystem::NOT_EXIST) {
            m_currentFileIndex = index;
            slotFound = true;
            break;
        }
    }

    if (!slotFound) {
        if (m_nextFileIndex >= maxFileCount) {
            this->markFileLimitReached();
        }
        m_recording = false;
        m_filesRemainingToRecord = 0;
        this->tlmWrite_Recording(m_recording);
        this->log_WARNING_HI_MaxFilesReached(maxFileCount);
        return false;
    }

    const Os::File::Status status = m_file.open(path.toChar(), Os::File::OPEN_CREATE, Os::File::NO_OVERWRITE);
    if (status != Os::File::OP_OK) {
        this->log_WARNING_HI_FileOpenError(path, static_cast<U32>(status));
        this->recordFilesystemError();
        m_nextFileIndex = m_currentFileIndex;
        return false;
    }

    m_fileOpen = true;
    if (m_nextFileIndex >= maxFileCount) {
        this->markFileLimitReached();
    }
    m_samplesInFile = 0;
    m_bufferedSamples = 0;
    return true;
}

void MosaicManager ::closeFile(bool complete) {
    Fw::FileNameString path;
    path.format("%s/gamma_%06u.dat", SAMPLE_DIR, m_currentFileIndex);

    if (complete && !this->writeBufferedSamples()) {
        complete = false;
    }

    m_file.flush();
    m_file.close();

    if (complete) {
        this->log_ACTIVITY_HI_SampleFileClosed(path, m_samplesInFile);
    }

    m_fileOpen = false;
    m_samplesInFile = 0;
    m_bufferedSamples = 0;
    if (complete) {
        m_filesWritten++;
        this->tlmWrite_FilesWritten(m_filesWritten);

        if (m_filesRemainingToRecord > 0) {
            m_filesRemainingToRecord--;
        }
        if (m_recording && (m_filesRemainingToRecord == 0)) {
            m_recording = false;
            this->tlmWrite_Recording(m_recording);
            this->log_ACTIVITY_HI_RecordingStopped();
        }
    }
}

void MosaicManager ::initializeFileIndex(U32 maxFileCount) {
    if (m_fileIndexInitialized) {
        return;
    }

    // Recover the cursor once after boot by locating the highest occupied
    // index. Holes below it remain unavailable until CLEAR_DIRECTORY.
    m_nextFileIndex = 0;
    for (U32 index = 0; index < maxFileCount; index++) {
        Fw::FileNameString path;
        path.format("%s/gamma_%06u.dat", SAMPLE_DIR, index);
        if (Os::FileSystem::getPathType(path.toChar()) != Os::FileSystem::NOT_EXIST) {
            m_nextFileIndex = index + 1;
        }
    }
    m_fileIndexInitialized = true;
    m_fileLimitReached = (Os::FileSystem::getPathType(FILE_LIMIT_MARKER) == Os::FileSystem::FILE);
    if (!m_fileLimitReached && (m_nextFileIndex >= maxFileCount)) {
        this->markFileLimitReached();
    }
}

U32 MosaicManager ::countRemainingFileSlots(U32 maxFileCount) {
    this->initializeFileIndex(maxFileCount);

    if (!m_fileLimitReached && (m_nextFileIndex >= maxFileCount)) {
        this->markFileLimitReached();
    }

    U32 remainingFileSlots = 0;
    if (!m_fileLimitReached && (m_nextFileIndex < maxFileCount)) {
        remainingFileSlots = maxFileCount - m_nextFileIndex;
    }
    if (m_fileOpen) {
        // The current partial file already occupies a slot, but it can still
        // satisfy one file in the newly requested recording run.
        remainingFileSlots++;
    }
    return remainingFileSlots;
}

void MosaicManager ::markFileLimitReached() {
    if (m_fileLimitReached) {
        return;
    }

    m_fileLimitReached = true;
    if (Os::FileSystem::getPathType(FILE_LIMIT_MARKER) == Os::FileSystem::FILE) {
        return;
    }

    Os::File markerFile;
    const Os::File::Status status = markerFile.open(FILE_LIMIT_MARKER, Os::File::OPEN_CREATE, Os::File::NO_OVERWRITE);
    if (status == Os::File::OP_OK) {
        markerFile.close();
        return;
    }

    const Fw::LogStringArg logFilePath(FILE_LIMIT_MARKER);
    const Fw::LogStringArg logOperation("create");
    this->log_WARNING_HI_FileOperationError(logFilePath, logOperation, static_cast<U32>(status));
    this->recordFilesystemError();
}

void MosaicManager ::recordFilesystemError() {
    m_filesystemErrors++;
    this->tlmWrite_FilesystemErrors(m_filesystemErrors);

    Fw::ParamValid paramValid;
    const U32 maxFilesystemErrors = this->paramGet_MAX_FILESYSTEM_ERRORS(paramValid);
    FW_ASSERT((paramValid == Fw::ParamValid::VALID) || (paramValid == Fw::ParamValid::DEFAULT));

    if (m_filesystemErrors >= maxFilesystemErrors) {
        m_recording = false;
        m_filesRemainingToRecord = 0;
        this->tlmWrite_Recording(m_recording);
        this->log_WARNING_HI_ErrorLimitReached(m_filesystemErrors);
    }
}

}  // namespace Components
