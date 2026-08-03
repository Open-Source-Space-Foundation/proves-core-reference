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
    if (m_fileOpen && m_samplesInFile > 0) {
        const U32 now = this->getTime().getSeconds();
        if ((now - m_fileStartSeconds) >= FLUSH_TIMEOUT_SECONDS) {
            this->closeFile();
        }
    }

    this->tlmWrite_Recording(m_recording);
    this->tlmWrite_SamplesRecorded(m_samplesRecorded);
    this->tlmWrite_FilesWritten(m_filesWritten);
    this->tlmWrite_ParseErrors(m_parseErrors);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void MosaicManager ::START_RECORDING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    m_recording = true;
    this->tlmWrite_Recording(m_recording);
    this->log_ACTIVITY_HI_RecordingStarted();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void MosaicManager ::STOP_RECORDING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    m_recording = false;
    if (m_fileOpen && m_samplesInFile > 0) {
        this->closeFile();
    }
    this->tlmWrite_Recording(m_recording);
    this->log_ACTIVITY_HI_RecordingStopped();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void MosaicManager ::FLUSH_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (m_fileOpen && m_samplesInFile > 0) {
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

    const U32 seconds = this->getTime().getSeconds();
    U8 record[RECORD_SIZE];
    std::memcpy(&record[0], &seconds, sizeof(seconds));
    std::memcpy(&record[sizeof(seconds)], &adc, sizeof(adc));
    std::memcpy(&record[sizeof(seconds) + sizeof(adc)], &millivolts, sizeof(millivolts));

    FwSizeType writeSize = RECORD_SIZE;
    const Os::File::Status status = m_file.write(record, writeSize, Os::File::WaitType::WAIT);
    if ((status != Os::File::OP_OK) || (writeSize != RECORD_SIZE)) {
        this->log_WARNING_HI_FileWriteError(static_cast<U32>(status));
        // A short write leaves a torn record on the end of the file, so the
        // file is abandoned rather than closed: reporting SampleFileClosed
        // here would advertise a clean, downlinkable file that isn't one.
        this->closeFile(false);
        return;
    }

    if (m_samplesInFile == 0) {
        m_fileStartSeconds = this->getTime().getSeconds();
    }
    m_samplesInFile++;
    m_samplesRecorded++;
    this->tlmWrite_SamplesRecorded(m_samplesRecorded);

    if (m_samplesInFile >= SAMPLES_PER_FILE) {
        this->closeFile();
    }
}

bool MosaicManager ::ensureFileOpen() {
    if (m_fileOpen) {
        return true;
    }

    // The Zephyr Os::File delegate truncates on OPEN_CREATE regardless of the NO_OVERWRITE flag
    // (its handling of `overwrite` is unimplemented), and m_filesWritten resets to 0 on every reboot.
    // Without this check, reusing a stale index would silently wipe a file from a previous boot that
    // has not yet been downlinked. Search forward for the first name not already on disk.
    Fw::FileNameString path;
    U32 searched = 0;
    do {
        path.format("%s/gamma_%06u.dat", SAMPLE_DIR, m_filesWritten);
        if (Os::FileSystem::getPathType(path.toChar()) == Os::FileSystem::NOT_EXIST) {
            break;
        }
        m_filesWritten++;
        searched++;
    } while (searched < MAX_FILE_INDEX_SEARCH);

    const Os::File::Status status = m_file.open(path.toChar(), Os::File::OPEN_CREATE, Os::File::NO_OVERWRITE);
    if (status != Os::File::OP_OK) {
        this->log_WARNING_HI_FileOpenError(path, static_cast<U32>(status));
        return false;
    }

    m_fileOpen = true;
    m_samplesInFile = 0;
    return true;
}

void MosaicManager ::closeFile(bool complete) {
    Fw::FileNameString path;
    path.format("%s/gamma_%06u.dat", SAMPLE_DIR, m_filesWritten);

    m_file.flush();
    m_file.close();

    if (complete) {
        this->log_ACTIVITY_HI_SampleFileClosed(path, m_samplesInFile);
    }

    m_fileOpen = false;
    m_samplesInFile = 0;
    m_filesWritten++;
    this->tlmWrite_FilesWritten(m_filesWritten);
}

}  // namespace Components
