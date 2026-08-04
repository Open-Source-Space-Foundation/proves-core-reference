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

void MosaicManager ::START_RECORDING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    m_filesystemErrors = 0;
    m_recording = true;
    this->tlmWrite_FilesystemErrors(m_filesystemErrors);
    this->tlmWrite_Recording(m_recording);
    this->log_ACTIVITY_HI_RecordingStarted();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void MosaicManager ::STOP_RECORDING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    m_recording = false;
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
        if (!this->ensureFileOpen()) {
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
        path.format("%s/gamma_%06u.dat", SAMPLE_DIR, m_filesWritten);
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

    // The Zephyr Os::File delegate truncates on OPEN_CREATE regardless of the NO_OVERWRITE flag
    // (its handling of `overwrite` is unimplemented), and m_filesWritten resets to 0 on every reboot.
    // Without this check, reusing a stale index would silently wipe a file from a previous boot that
    // has not yet been downlinked. Search forward for the first name not already on disk.
    Fw::FileNameString path;
    while (m_filesWritten < maxFileCount) {
        path.format("%s/gamma_%06u.dat", SAMPLE_DIR, m_filesWritten);
        if (Os::FileSystem::getPathType(path.toChar()) == Os::FileSystem::NOT_EXIST) {
            break;
        }
        m_filesWritten++;
    }

    if (m_filesWritten >= maxFileCount) {
        m_recording = false;
        this->tlmWrite_Recording(m_recording);
        this->log_WARNING_HI_MaxFilesReached(maxFileCount);
        return false;
    }

    const Os::File::Status status = m_file.open(path.toChar(), Os::File::OPEN_CREATE, Os::File::NO_OVERWRITE);
    if (status != Os::File::OP_OK) {
        this->log_WARNING_HI_FileOpenError(path, static_cast<U32>(status));
        this->recordFilesystemError();
        return false;
    }

    m_fileOpen = true;
    m_samplesInFile = 0;
    m_bufferedSamples = 0;
    return true;
}

void MosaicManager ::closeFile(bool complete) {
    Fw::FileNameString path;
    path.format("%s/gamma_%06u.dat", SAMPLE_DIR, m_filesWritten);

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
    m_filesWritten++;
    this->tlmWrite_FilesWritten(m_filesWritten);
}

void MosaicManager ::recordFilesystemError() {
    m_filesystemErrors++;
    this->tlmWrite_FilesystemErrors(m_filesystemErrors);

    Fw::ParamValid paramValid;
    const U32 maxFilesystemErrors = this->paramGet_MAX_FILESYSTEM_ERRORS(paramValid);
    FW_ASSERT((paramValid == Fw::ParamValid::VALID) || (paramValid == Fw::ParamValid::DEFAULT));

    if (m_filesystemErrors >= maxFilesystemErrors) {
        m_recording = false;
        this->tlmWrite_Recording(m_recording);
        this->log_WARNING_HI_ErrorLimitReached(m_filesystemErrors);
    }
}

}  // namespace Components
