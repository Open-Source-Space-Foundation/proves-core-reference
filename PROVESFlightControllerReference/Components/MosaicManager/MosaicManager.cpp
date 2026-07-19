// ======================================================================
// \title  MosaicManager.cpp
// \brief  cpp file for MosaicManager component implementation class
//         Receives MOSAIC gamma ray detector data over UART and stores
//         it on disk as F Prime data products
// ======================================================================

#include "PROVESFlightControllerReference/Components/MosaicManager/MosaicManager.hpp"

#include <cstdlib>
#include <cstring>

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
    // Flush a partially filled container that has been sitting too long
    if (m_containerOpen && m_samplesInContainer > 0) {
        const U32 now = this->getTime().getSeconds();
        if ((now - m_containerStartSeconds) >= FLUSH_TIMEOUT_SECONDS) {
            this->sendContainer();
        }
    }

    this->tlmWrite_Recording(m_recording);
    this->tlmWrite_SamplesRecorded(m_samplesRecorded);
    this->tlmWrite_ProductsSent(m_productsSent);
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
    if (m_containerOpen && m_samplesInContainer > 0) {
        this->sendContainer();
    }
    this->tlmWrite_Recording(m_recording);
    this->log_ACTIVITY_HI_RecordingStopped();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void MosaicManager ::FLUSH_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (m_containerOpen && m_samplesInContainer > 0) {
        this->sendContainer();
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

    const MosaicManager_MosaicSample sample(static_cast<U16>(adc), static_cast<U16>(millivolts));
    this->tlmWrite_LatestAdc(sample.get_adc());
    this->tlmWrite_LatestMillivolts(sample.get_millivolts());

    if (m_recording) {
        this->recordSample(sample);
    }
}

void MosaicManager ::recordSample(const MosaicManager_MosaicSample& sample) {
    if (!this->ensureContainer()) {
        return;
    }

    const Fw::SerializeStatus status = m_container.serializeRecord_SampleRecord(sample);
    if (status != Fw::FW_SERIALIZE_OK) {
        this->log_WARNING_HI_RecordSerializeError();
        this->sendContainer();
        return;
    }

    if (m_samplesInContainer == 0) {
        m_containerStartSeconds = this->getTime().getSeconds();
    }
    m_samplesInContainer++;
    m_samplesRecorded++;
    this->tlmWrite_SamplesRecorded(m_samplesRecorded);

    if (m_samplesInContainer >= SAMPLES_PER_CONTAINER) {
        this->sendContainer();
    }
}

bool MosaicManager ::ensureContainer() {
    if (m_containerOpen) {
        return true;
    }

    const FwSizeType dataSize = static_cast<FwSizeType>(SAMPLES_PER_CONTAINER) * RECORD_SIZE;
    const Fw::Success status = this->dpGet_GammaData(dataSize, m_container);
    if (status != Fw::Success::SUCCESS) {
        this->log_WARNING_HI_DpMemoryFail();
        return false;
    }

    m_containerOpen = true;
    m_samplesInContainer = 0;
    return true;
}

void MosaicManager ::sendContainer() {
    this->dpSend(m_container);
    this->log_ACTIVITY_HI_DataProductSent(m_samplesInContainer);

    m_containerOpen = false;
    m_samplesInContainer = 0;
    m_productsSent++;
    this->tlmWrite_ProductsSent(m_productsSent);
}

}  // namespace Components
