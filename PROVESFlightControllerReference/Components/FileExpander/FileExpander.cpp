// ======================================================================
// \title  FileExpander.cpp
// \brief  Rebuilds UPZ-compressed uplinked files in bounded rate-group steps
// ======================================================================

#include "PROVESFlightControllerReference/Components/FileExpander/FileExpander.hpp"

namespace Components {

namespace {

bool mediaOk(Update::DeltaMedia::Status status) {
    return status == Update::DeltaMedia::OP_OK;
}

}  // namespace

// ----------------------------------------------------------------------
// MediaIo
// ----------------------------------------------------------------------

bool FileExpander::MediaIo::open(const char* coded, const char* out) {
    this->close();
    this->m_outPath = out;
    return this->m_coded.open(coded, Update::DeltaFileMedia::READ_ONLY) == Os::File::OP_OK &&
           this->m_out.open(out, Update::DeltaFileMedia::READ_WRITE) == Os::File::OP_OK;
}

void FileExpander::MediaIo::close() {
    this->m_coded.close();
    this->m_out.close();
}

bool FileExpander::MediaIo::codedSize(uint64_t& size) {
    FwSizeType s = 0;
    const bool ok = mediaOk(this->m_coded.size(s));
    size = static_cast<uint64_t>(s);
    return ok;
}

bool FileExpander::MediaIo::readCoded(uint64_t offset, uint8_t* buffer, uint32_t length) {
    return mediaOk(this->m_coded.read(static_cast<FwSizeType>(offset), buffer, length));
}

bool FileExpander::MediaIo::outSize(uint64_t& size) {
    FwSizeType s = 0;
    const bool ok = mediaOk(this->m_out.size(s));
    size = static_cast<uint64_t>(s);
    return ok;
}

bool FileExpander::MediaIo::readOut(uint64_t offset, uint8_t* buffer, uint32_t length) {
    return mediaOk(this->m_out.read(static_cast<FwSizeType>(offset), buffer, length));
}

bool FileExpander::MediaIo::writeOut(uint64_t offset, const uint8_t* buffer, uint32_t length) {
    return mediaOk(this->m_out.write(static_cast<FwSizeType>(offset), buffer, length));
}

bool FileExpander::MediaIo::flushOut() {
    return mediaOk(this->m_out.flush());
}

bool FileExpander::MediaIo::resetOut() {
    this->m_out.close();
    return this->m_out.open(this->m_outPath.toChar(), Update::DeltaFileMedia::CREATE) == Os::File::OP_OK;
}

// ----------------------------------------------------------------------
// Component
// ----------------------------------------------------------------------

FileExpander::FileExpander(const char* const compName)
    : FileExpanderComponentBase(compName), m_lastPhase(Upz::Phase::IDLE), m_ticks(0) {}

FileExpander::~FileExpander() {}

bool FileExpander::busy() const {
    const Upz::Phase phase = this->m_expander.phase();
    return phase != Upz::Phase::IDLE && phase != Upz::Phase::DONE && phase != Upz::Phase::FAILED;
}

void FileExpander::run_handler(FwIndexType portNum, U32 context) {
    for (FwSizeType i = 0; i < MAX_DISPATCH_PER_TICK; i++) {
        if (this->doDispatch() == MSG_DISPATCH_EMPTY) {
            break;
        }
    }
    if (!this->busy()) {
        return;
    }
    this->m_ticks++;
    const Upz::Phase phase = this->m_expander.step(OUT_BYTES_PER_STEP, IO_PER_STEP);
    if (phase == Upz::Phase::DECODE && this->m_lastPhase != Upz::Phase::DECODE) {
        this->log_ACTIVITY_HI_ExpandResumed(this->m_expander.resumeChunk(), this->m_expander.chunkCount(),
                                            this->m_expander.rawSize());
    }
    this->m_lastPhase = phase;
    this->tlmWrite_Phase(static_cast<ExpandPhase::T>(phase));
    this->tlmWrite_Chunk(this->m_expander.chunkIndex());
    this->tlmWrite_BytesDone(this->m_expander.outPosition());
    if (phase == Upz::Phase::DONE || phase == Upz::Phase::FAILED) {
        this->finish();
    }
}

void FileExpander::finish() {
    this->m_io.close();
    const Upz::Status status = this->m_expander.status();
    this->tlmWrite_LastStatus(static_cast<ExpandStatus::T>(status));
    if (this->m_expander.phase() == Upz::Phase::DONE) {
        this->log_ACTIVITY_HI_ExpandComplete(Fw::LogStringArg(this->m_outFile.toChar()), this->m_expander.rawSize(),
                                             this->m_expander.rawCrc(), this->m_expander.farReads(), this->m_ticks);
    } else {
        this->log_WARNING_HI_ExpandFailed(static_cast<ExpandStatus::T>(status), this->m_expander.chunkIndex());
    }
}

void FileExpander::EXPAND_FILE_cmdHandler(FwOpcodeType opCode,
                                          U32 cmdSeq,
                                          const Fw::CmdStringArg& coded_file,
                                          const Fw::CmdStringArg& out_file) {
    if (this->busy()) {
        this->log_WARNING_HI_ExpandFailed(ExpandStatus::BUSY, this->m_expander.chunkIndex());
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
        return;
    }
    if (coded_file == out_file || !Update::DeltaFileMedia::isRegularFile(coded_file.toChar()) ||
        !this->m_io.open(coded_file.toChar(), out_file.toChar())) {
        this->m_io.close();
        this->log_WARNING_HI_ExpandFailed(ExpandStatus::BAD_PATH, 0);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    this->m_outFile = out_file.toChar();
    this->m_ticks = 0;
    this->m_lastPhase = Upz::Phase::IDLE;
    this->m_expander.begin(this->m_io);
    this->log_ACTIVITY_HI_ExpandStarted(Fw::LogStringArg(coded_file.toChar()), Fw::LogStringArg(out_file.toChar()));
    this->tlmWrite_LastStatus(ExpandStatus::OK);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void FileExpander::ABORT_EXPAND_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (this->busy()) {
        this->log_ACTIVITY_HI_ExpandAborted(this->m_expander.chunkIndex());
        this->m_expander.abort();
        this->m_io.close();
        this->m_lastPhase = Upz::Phase::IDLE;
        this->tlmWrite_Phase(ExpandPhase::IDLE);
    }
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
