// ======================================================================
// \title  FlashWorker.cpp
// \author starchmd
// \brief  cpp file for FlashWorker component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/FlashWorker/FlashWorker.hpp"

#include <Utils/Hash/libcrc/lib_crc.h>  // same CRC primitive Os::File::calculateCrc uses

#include <limits>

#include "Os/File.hpp"
#include "Os/Task.hpp"
#include <zephyr/dfu/flash_img.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/storage/flash_map.h>

namespace {
//! Flash area holding the running image, used as the reference a delta patch is applied to
constexpr U8 RUNNING_IMAGE_REGION = FIXED_PARTITION_ID(slot0_partition);

//! Seed for an incremental CRC32.
//!
//! Must equal Os::File::INITIAL_CRC, which is private, so that a CRC accumulated here over data
//! already in memory is directly comparable to one from Os::File::calculateCrc and to the value
//! tools/bin/calculate-crc.py reports on the ground.
constexpr U32 FILE_CRC_SEED = 0xFFFFFFFF;
}  // namespace

namespace Components {

// UpdateSequencer mirrors Update.FlashWorkerUpdateStatus so that the update sequencing logic can be
// unit tested without F Prime. Keep the two in lockstep; reordering either is a compile error here.
static_assert(static_cast<U8>(UpdateSequencer::Status::OP_OK) == static_cast<U8>(Update::UpdateStatus::OP_OK),
              "UpdateSequencer::Status::OP_OK must match Update::UpdateStatus::OP_OK");
static_assert(static_cast<U8>(UpdateSequencer::Status::BUSY) == static_cast<U8>(Update::UpdateStatus::BUSY),
              "UpdateSequencer::Status::BUSY must match Update::UpdateStatus::BUSY");
static_assert(static_cast<U8>(UpdateSequencer::Status::UNPREPARED) == static_cast<U8>(Update::UpdateStatus::UNPREPARED),
              "UpdateSequencer::Status::UNPREPARED must match Update::UpdateStatus::UNPREPARED");
static_assert(static_cast<U8>(UpdateSequencer::Status::PREPARATION_ERROR) ==
                  static_cast<U8>(Update::UpdateStatus::PREPARATION_ERROR),
              "UpdateSequencer::Status::PREPARATION_ERROR must match Update::UpdateStatus::PREPARATION_ERROR");
static_assert(static_cast<U8>(UpdateSequencer::Status::IMAGE_FILE_READ_ERROR) ==
                  static_cast<U8>(Update::UpdateStatus::IMAGE_FILE_READ_ERROR),
              "UpdateSequencer::Status::IMAGE_FILE_READ_ERROR must match Update::UpdateStatus::IMAGE_FILE_READ_ERROR");
static_assert(static_cast<U8>(UpdateSequencer::Status::IMAGE_CRC_MISMATCH) ==
                  static_cast<U8>(Update::UpdateStatus::IMAGE_CRC_MISMATCH),
              "UpdateSequencer::Status::IMAGE_CRC_MISMATCH must match Update::UpdateStatus::IMAGE_CRC_MISMATCH");
static_assert(static_cast<U8>(UpdateSequencer::Status::NEXT_BOOT_ERROR) ==
                  static_cast<U8>(Update::UpdateStatus::NEXT_BOOT_ERROR),
              "UpdateSequencer::Status::NEXT_BOOT_ERROR must match Update::UpdateStatus::NEXT_BOOT_ERROR");
static_assert(static_cast<U8>(UpdateSequencer::Status::FLASH_WRITE_ERROR) ==
                  static_cast<U8>(Update::UpdateStatus::FLASH_WRITE_ERROR),
              "UpdateSequencer::Status::FLASH_WRITE_ERROR must match Update::UpdateStatus::FLASH_WRITE_ERROR");

// The sequencer's stage mirrors Components.FlashUpdateStage in FlashWorker.fpp
static_assert(static_cast<U8>(UpdateSequencer::Stage::IDLE) == static_cast<U8>(Components::FlashUpdateStage::IDLE),
              "UpdateSequencer::Stage::IDLE must match FlashUpdateStage::IDLE");
static_assert(static_cast<U8>(UpdateSequencer::Stage::PREPARING) ==
                  static_cast<U8>(Components::FlashUpdateStage::PREPARING),
              "UpdateSequencer::Stage::PREPARING must match FlashUpdateStage::PREPARING");
static_assert(static_cast<U8>(UpdateSequencer::Stage::PREPARED) ==
                  static_cast<U8>(Components::FlashUpdateStage::PREPARED),
              "UpdateSequencer::Stage::PREPARED must match FlashUpdateStage::PREPARED");
static_assert(static_cast<U8>(UpdateSequencer::Stage::WRITING) ==
                  static_cast<U8>(Components::FlashUpdateStage::WRITING),
              "UpdateSequencer::Stage::WRITING must match FlashUpdateStage::WRITING");
static_assert(static_cast<U8>(UpdateSequencer::Stage::UPDATED) ==
                  static_cast<U8>(Components::FlashUpdateStage::UPDATED),
              "UpdateSequencer::Stage::UPDATED must match FlashUpdateStage::UPDATED");
static_assert(static_cast<U8>(UpdateSequencer::Stage::FAILED) == static_cast<U8>(Components::FlashUpdateStage::FAILED),
              "UpdateSequencer::Stage::FAILED must match FlashUpdateStage::FAILED");

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

FlashWorker ::FlashWorker(const char* const compName)
    : FlashWorkerComponentBase(compName), m_pending_confirm_seconds(0), m_last_reported_percent(0) {}

FlashWorker ::~FlashWorker() {}

// ----------------------------------------------------------------------
// Flash helpers
// ----------------------------------------------------------------------

Update::UpdateStatus FlashWorker ::toUpdateStatus(UpdateSequencer::Status status) {
    return static_cast<Update::UpdateStatus::T>(static_cast<U8>(status));
}

Components::FlashUpdateStage FlashWorker ::toUpdateStage(UpdateSequencer::Stage stage) {
    return static_cast<Components::FlashUpdateStage::T>(static_cast<U8>(stage));
}

void FlashWorker ::reportStage(Components::FlashUpdateStage stage, Update::UpdateStatus status) {
    this->tlmWrite_UpdateStage(stage);
    this->tlmWrite_LastUpdateStatus(status);
}

void FlashWorker ::reportProgress(U32 written, U32 total) {
    this->tlmWrite_BytesWritten(written);
    const U8 percent = UpdateSequencer::percentComplete(written, total);

    Fw::ParamValid valid = Fw::ParamValid::INVALID;
    const U8 step = this->paramGet_PROGRESS_STEP_PERCENT(valid);
    if (UpdateSequencer::progressReportDue(percent, this->m_last_reported_percent, step)) {
        this->log_ACTIVITY_LO_UpdateProgress(written, total, percent);
        this->m_last_reported_percent = percent;
    }
}

UpdateSequencer::WriteOutcome FlashWorker ::writeImage(const Fw::StringBase& file_name,
                                                       Os::File& file,
                                                       U32 expected_crc32) {
    const FwSizeType CHUNK = static_cast<FwSizeType>(sizeof(this->m_data));
    FW_ASSERT(file.isOpen());
    FwSizeType size = 0;
    U32 file_crc = 0;

    // Read file size, needed to bound the write loop
    Os::File::Status file_status = file.size(size);
    if (file_status != Os::File::Status::OP_OK) {
        this->log_WARNING_LO_ImageFileReadError(file_name, Os::FileStatus(static_cast<Os::FileStatus::T>(file_status)));
        return UpdateSequencer::WriteOutcome::FILE_QUERY_FAILED;
    }

    // Validate the file before touching the flash, so that a bad image never displaces the erased
    // staging slot and the operator can retry without paying for another erase
    file_status = file.calculateCrc(file_crc);
    if (file_status != Os::File::Status::OP_OK || file_crc != expected_crc32) {
        this->log_WARNING_LO_ImageFileCrcMismatch(
            file_name, Os::FileStatus(static_cast<Os::FileStatus::T>(file_status)), expected_crc32, file_crc);
        return (file_status != Os::File::Status::OP_OK) ? UpdateSequencer::WriteOutcome::FILE_QUERY_FAILED
                                                        : UpdateSequencer::WriteOutcome::CRC_MISMATCH;
    }

    // calculateCrc leaves the cursor at the end of the file; rewind before streaming it out
    file_status = file.seek(0, Os::File::SeekType::ABSOLUTE);
    if (file_status != Os::File::Status::OP_OK) {
        this->log_WARNING_LO_ImageFileReadError(file_name, Os::FileStatus(static_cast<Os::FileStatus::T>(file_status)));
        return UpdateSequencer::WriteOutcome::FILE_QUERY_FAILED;
    }

    int status = flash_img_init_id(&this->m_flash_context, FlashWorker::REGION_NUMBER);
    if (status != 0) {
        this->log_WARNING_LO_FlashWriteFailed(static_cast<I32>(-1 * status), 0);
        return UpdateSequencer::WriteOutcome::FLASH_WRITE_FAILED;
    }

    Fw::ParamValid valid = Fw::ParamValid::INVALID;
    const U32 chunk_delay_us = this->paramGet_CHUNK_DELAY_US(valid);

    // CRC of the bytes actually streamed out to the flash. Computed with the same primitive and
    // seed as Os::File::calculateCrc so that the two are directly comparable, and free because the
    // data is already in the buffer. It catches the image changing underneath us or an unstable
    // filesystem read, neither of which the pre-write validation above can see.
    U32 written_crc = FILE_CRC_SEED;
    FwSizeType written = 0;

    // Loop through file chunk by chunk
    for (FwSizeType i = 0; i < size; i += CHUNK) {
        FwSizeType read_size = CHUNK;
        file_status = file.read(this->m_data, read_size);
        if (file_status != Os::File::Status::OP_OK) {
            this->log_WARNING_LO_ImageFileReadError(file_name,
                                                    Os::FileStatus(static_cast<Os::FileStatus::T>(file_status)));
            return UpdateSequencer::WriteOutcome::FILE_READ_FAILED;
        }
        // The file ended earlier than its reported size; stop rather than flushing empty writes
        if (read_size == 0) {
            break;
        }
        status = flash_img_buffered_write(&this->m_flash_context, this->m_data, read_size, true);
        if (status != 0) {
            this->log_WARNING_LO_FlashWriteFailed(static_cast<I32>(-1 * status), i);
            return UpdateSequencer::WriteOutcome::FLASH_WRITE_FAILED;
        }
        for (FwSizeType byte = 0; byte < read_size; byte++) {
            written_crc = static_cast<U32>(update_crc_32(written_crc, static_cast<CHAR>(this->m_data[byte])));
        }
        written += read_size;
        this->reportProgress(static_cast<U32>(written), static_cast<U32>(size));

        // Give the flash time to process the data and allow more to be loaded off the filesystem
        if (chunk_delay_us > 0) {
            Os::Task::delay(Fw::TimeInterval(0, chunk_delay_us));
        }
    }

    // What landed in the slot must match what was validated before the write started
    if (written_crc != expected_crc32) {
        this->log_WARNING_HI_ImageWriteCrcMismatch(expected_crc32, written_crc);
        return UpdateSequencer::WriteOutcome::FLASH_WRITE_FAILED;
    }
    return UpdateSequencer::WriteOutcome::SUCCESS;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Update::UpdateStatus FlashWorker ::confirmImage_handler(FwIndexType portNum) {
    int status = boot_write_img_confirmed();
    if (status != 0) {
        this->log_WARNING_LO_ConfirmImageFailed(static_cast<I32>(-1 * status));
        return Update::UpdateStatus::NEXT_BOOT_ERROR;
    }
    return Update::UpdateStatus::OP_OK;
}

Update::UpdateStatus FlashWorker ::nextBoot_handler(FwIndexType portNum, const Update::NextBootMode& mode) {
    int permanent = (mode == Update::NextBootMode::PERMANENT) ? BOOT_UPGRADE_PERMANENT : BOOT_UPGRADE_TEST;

    int status = boot_request_upgrade(permanent);
    if (status != 0) {
        this->log_WARNING_LO_NextBootSetFailed(mode, static_cast<I32>(-1 * status));
        return Update::UpdateStatus::NEXT_BOOT_ERROR;
    }
    return Update::UpdateStatus::OP_OK;
}

void FlashWorker ::prepareImage_handler(FwIndexType portNum) {
    // The erase is slow enough that an operator needs to see it started, not just finished
    this->reportStage(Components::FlashUpdateStage::PREPARING, Update::UpdateStatus::OP_OK);

    int status = boot_erase_img_bank(FlashWorker::REGION_NUMBER);
    if (status != 0) {
        this->log_WARNING_LO_FlashEraseFailed(static_cast<I32>(-1 * status));
    }
    const Update::UpdateStatus return_status =
        FlashWorker::toUpdateStatus(this->m_sequencer.onPrepareComplete(status == 0));

    this->tlmWrite_BytesWritten(0);
    this->tlmWrite_ImageTotalBytes(0);
    this->reportStage(FlashWorker::toUpdateStage(this->m_sequencer.settledStage()), return_status);
    this->prepareImageDone_out(0, return_status);
}

void FlashWorker ::updateImage_handler(FwIndexType portNum, const Fw::StringBase& file, U32 crc32) {
    if (!this->m_sequencer.isPrepared()) {
        this->log_WARNING_LO_NoImagePrepared();
        const Update::UpdateStatus return_status = FlashWorker::toUpdateStatus(UpdateSequencer::Status::UNPREPARED);
        // The sequence itself is untouched by a rejected request, so only the status is reported
        this->tlmWrite_LastUpdateStatus(return_status);
        this->updateImageDone_out(0, return_status);
        return;
    }

    Os::File image_file;
    UpdateSequencer::WriteOutcome outcome = UpdateSequencer::WriteOutcome::FILE_OPEN_FAILED;

    this->m_last_reported_percent = 0;
    this->tlmWrite_BytesWritten(0);
    this->reportStage(Components::FlashUpdateStage::WRITING, Update::UpdateStatus::OP_OK);

    Os::File::Status file_status = image_file.open(file.toChar(), Os::File::Mode::OPEN_READ);
    if (file_status == Os::File::Status::OP_OK) {
        FwSizeType total = 0;
        if (image_file.size(total) == Os::File::Status::OP_OK) {
            this->tlmWrite_ImageTotalBytes(static_cast<U32>(total));
        }
        outcome = this->writeImage(file, image_file, crc32);
    } else {
        this->log_WARNING_LO_ImageFileReadError(file, static_cast<Os::FileStatus::T>(file_status));
    }

    const Update::UpdateStatus return_status = FlashWorker::toUpdateStatus(this->m_sequencer.onUpdateComplete(outcome));
    this->reportStage(FlashWorker::toUpdateStage(this->m_sequencer.settledStage()), return_status);
    this->updateImageDone_out(0, return_status);
}

void FlashWorker ::run_handler(FwIndexType portNum, U32 context) {
    // boot_is_img_confirmed reports whether the running image is already the permanent choice.
    // While it is false this is a test boot that reverts on the next reboot unless confirmed.
    const bool confirmed = (boot_is_img_confirmed() != 0);
    this->tlmWrite_RunningImageConfirmed(confirmed);

    if (confirmed) {
        this->m_pending_confirm_seconds = 0;
        this->tlmWrite_PendingConfirmSeconds(0);
        return;
    }

    // Saturate rather than wrap, so a very long unconfirmed run cannot roll back under the delay
    if (this->m_pending_confirm_seconds < std::numeric_limits<U32>::max()) {
        this->m_pending_confirm_seconds++;
    }
    this->tlmWrite_PendingConfirmSeconds(this->m_pending_confirm_seconds);

    Fw::ParamValid enabled_valid = Fw::ParamValid::INVALID;
    Fw::ParamValid delay_valid = Fw::ParamValid::INVALID;
    const bool enabled = this->paramGet_AUTO_CONFIRM_ENABLED(enabled_valid);
    const U32 delay = this->paramGet_AUTO_CONFIRM_DELAY_SECONDS(delay_valid);

    if (!UpdateSequencer::autoConfirmDue(enabled, confirmed, this->m_pending_confirm_seconds, delay)) {
        return;
    }

    const int status = boot_write_img_confirmed();
    if (status != 0) {
        this->log_WARNING_HI_AutoConfirmFailed(static_cast<I32>(-1 * status));
        // Leave the counter alone so the next tick retries; a transient flash error should not
        // cost the image its chance to be kept
        return;
    }
    this->log_ACTIVITY_HI_AutoConfirmed(this->m_pending_confirm_seconds);
    this->m_pending_confirm_seconds = 0;
}

// ----------------------------------------------------------------------
// Command handler implementations
// ----------------------------------------------------------------------

void FlashWorker ::ASSEMBLE_IMAGE_cmdHandler(FwOpcodeType opCode,
                                             U32 cmdSeq,
                                             const Fw::CmdStringArg& prefix,
                                             U16 segments,
                                             const Fw::CmdStringArg& destination,
                                             U32 crc32) {
    if (!SegmentPlan::isValidSegmentCount(segments)) {
        this->log_WARNING_HI_InvalidSegmentCount(segments);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    this->log_ACTIVITY_HI_AssembleStarted(segments, destination);

    Os::File assembled;
    if (assembled.open(destination.toChar(), Os::File::Mode::OPEN_CREATE, Os::File::OverwriteType::OVERWRITE) !=
        Os::File::Status::OP_OK) {
        this->log_WARNING_HI_AssembleFailed(0, Update::UpdateStatus::IMAGE_FILE_READ_ERROR);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    U32 total = 0;
    for (U16 segment = 0; segment < segments; segment++) {
        char name[FileNameStringSize];
        if (!SegmentPlan::formatSegmentName(prefix.toChar(), segment, name, sizeof(name))) {
            this->log_WARNING_HI_AssembleFailed(segment, Update::UpdateStatus::IMAGE_FILE_READ_ERROR);
            this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
            return;
        }
        Os::File piece;
        if (piece.open(name, Os::File::Mode::OPEN_READ) != Os::File::Status::OP_OK) {
            this->log_WARNING_HI_AssembleFailed(segment, Update::UpdateStatus::IMAGE_FILE_READ_ERROR);
            this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
            return;
        }
        // Stream the segment through the existing chunk buffer rather than sizing a new one
        while (true) {
            FwSizeType chunk = static_cast<FwSizeType>(sizeof(this->m_data));
            if (piece.read(this->m_data, chunk) != Os::File::Status::OP_OK) {
                this->log_WARNING_HI_AssembleFailed(segment, Update::UpdateStatus::IMAGE_FILE_READ_ERROR);
                this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
                return;
            }
            if (chunk == 0) {
                break;
            }
            FwSizeType written = chunk;
            if ((assembled.write(this->m_data, written) != Os::File::Status::OP_OK) || (written != chunk)) {
                this->log_WARNING_HI_AssembleFailed(segment, Update::UpdateStatus::IMAGE_FILE_READ_ERROR);
                this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
                return;
            }
            total += static_cast<U32>(written);
        }
    }
    assembled.close();

    // Validate the joined image before anyone tries to flash it, so a missing or reordered
    // segment is caught here rather than by the bootloader
    Os::File verify;
    U32 actual_crc = 0;
    if ((verify.open(destination.toChar(), Os::File::Mode::OPEN_READ) != Os::File::Status::OP_OK) ||
        (verify.calculateCrc(actual_crc) != Os::File::Status::OP_OK)) {
        this->log_WARNING_HI_AssembleFailed(segments, Update::UpdateStatus::IMAGE_FILE_READ_ERROR);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    if (actual_crc != crc32) {
        this->log_WARNING_LO_ImageFileCrcMismatch(destination, Os::FileStatus(Os::FileStatus::T::OP_OK), crc32,
                                                  actual_crc);
        this->log_WARNING_HI_AssembleFailed(segments, Update::UpdateStatus::IMAGE_CRC_MISMATCH);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    this->log_ACTIVITY_HI_AssembleSucceeded(total);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

//! Sources and sink for a patch apply: the running image in flash, three cursors into the patch
//! file, and the reconstructed image on the filesystem.
class FlashWorker::PatchIo final : public PatchApplier::Io {
  public:
    PatchIo(const struct flash_area* area, Os::File& control, Os::File& diff, Os::File& extra, Os::File& out)
        : m_area(area), m_control(control), m_diff(diff), m_extra(extra), m_out(out) {}

    bool readReference(uint32_t offset, uint8_t* buffer, size_t size) override {
        return flash_area_read(this->m_area, static_cast<off_t>(offset), buffer, size) == 0;
    }
    bool readControl(uint8_t* buffer, size_t size) override { return readExactly(this->m_control, buffer, size); }
    bool readDiff(uint8_t* buffer, size_t size) override { return readExactly(this->m_diff, buffer, size); }
    bool readExtra(uint8_t* buffer, size_t size) override { return readExactly(this->m_extra, buffer, size); }
    bool writeOutput(const uint8_t* buffer, size_t size) override {
        FwSizeType written = static_cast<FwSizeType>(size);
        return (this->m_out.write(buffer, written) == Os::File::Status::OP_OK) &&
               (written == static_cast<FwSizeType>(size));
    }

  private:
    static bool readExactly(Os::File& file, uint8_t* buffer, size_t size) {
        FwSizeType requested = static_cast<FwSizeType>(size);
        return (file.read(buffer, requested) == Os::File::Status::OP_OK) &&
               (requested == static_cast<FwSizeType>(size));
    }

    const struct flash_area* m_area;
    Os::File& m_control;
    Os::File& m_diff;
    Os::File& m_extra;
    Os::File& m_out;
};

//! Compressed patch bytes, read sequentially from the patch file
class FlashWorker::PatchSource final : public LzssDecoder::Source {
  public:
    explicit PatchSource(Os::File& file) : m_file(file) {}
    bool read(uint8_t* buffer, size_t size) override {
        FwSizeType requested = static_cast<FwSizeType>(size);
        return (this->m_file.read(buffer, requested) == Os::File::Status::OP_OK) &&
               (requested == static_cast<FwSizeType>(size));
    }

  private:
    Os::File& m_file;
};

//! Decoded patch bytes, appended to the scratch file
class FlashWorker::PatchSink final : public LzssDecoder::Sink {
  public:
    explicit PatchSink(Os::File& file) : m_file(file) {}
    bool write(const uint8_t* buffer, size_t size) override {
        FwSizeType requested = static_cast<FwSizeType>(size);
        return (this->m_file.write(buffer, requested) == Os::File::Status::OP_OK) &&
               (requested == static_cast<FwSizeType>(size));
    }

  private:
    Os::File& m_file;
};

bool FlashWorker ::decompressPatch(const Fw::StringBase& patch, const char* scratch_path, U32 expected_size) {
    Os::File compressed;
    Os::File plain;
    if (compressed.open(patch.toChar(), Os::File::Mode::OPEN_READ) != Os::File::Status::OP_OK) {
        return false;
    }
    // Skip the header; the payload is everything after it
    if (compressed.seek(static_cast<FwSizeType>(PatchApplier::HEADER_SIZE), Os::File::SeekType::ABSOLUTE) !=
        Os::File::Status::OP_OK) {
        return false;
    }
    if (plain.open(scratch_path, Os::File::Mode::OPEN_CREATE, Os::File::OverwriteType::OVERWRITE) !=
        Os::File::Status::OP_OK) {
        return false;
    }
    PatchSource source(compressed);
    PatchSink sink(plain);
    const LzssDecoder::Error error = LzssDecoder::decode(source, sink, expected_size, this->m_window);
    plain.close();
    return error == LzssDecoder::Error::NONE;
}

void FlashWorker ::APPLY_PATCH_cmdHandler(FwOpcodeType opCode,
                                          U32 cmdSeq,
                                          const Fw::CmdStringArg& patch,
                                          const Fw::CmdStringArg& destination,
                                          U32 crc32) {
    this->log_ACTIVITY_HI_PatchStarted(patch, destination);

    Os::File header_file;
    if (header_file.open(patch.toChar(), Os::File::Mode::OPEN_READ) != Os::File::Status::OP_OK) {
        this->log_WARNING_HI_PatchFailed(static_cast<U8>(PatchApplier::Error::PATCH_READ_FAILED));
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    U8 header_bytes[PatchApplier::HEADER_SIZE];
    FwSizeType header_size = static_cast<FwSizeType>(sizeof(header_bytes));
    if ((header_file.read(header_bytes, header_size) != Os::File::Status::OP_OK) ||
        (header_size != static_cast<FwSizeType>(sizeof(header_bytes)))) {
        this->log_WARNING_HI_PatchFailed(static_cast<U8>(PatchApplier::Error::TRUNCATED_PATCH));
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    header_file.close();

    PatchApplier::Header header;
    PatchApplier::Error error = PatchApplier::decodeHeader(header_bytes, sizeof(header_bytes), header);
    if (error != PatchApplier::Error::NONE) {
        this->log_WARNING_HI_PatchFailed(static_cast<U8>(error));
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    const struct flash_area* area = nullptr;
    if (flash_area_open(RUNNING_IMAGE_REGION, &area) != 0) {
        this->log_WARNING_HI_PatchFailed(static_cast<U8>(PatchApplier::Error::REFERENCE_READ_FAILED));
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // Prove the running image is the one the ground diffed against. Patching a different image
    // produces a plausible but corrupt result that would then be flashed and booted.
    U32 reference_crc = FILE_CRC_SEED;
    for (U32 offset = 0; offset < header.reference_size;) {
        const U32 remaining = header.reference_size - offset;
        const size_t chunk = (remaining < sizeof(this->m_data)) ? remaining : sizeof(this->m_data);
        if (flash_area_read(area, static_cast<off_t>(offset), this->m_data, chunk) != 0) {
            flash_area_close(area);
            this->log_WARNING_HI_PatchFailed(static_cast<U8>(PatchApplier::Error::REFERENCE_READ_FAILED));
            this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
            return;
        }
        for (size_t i = 0; i < chunk; i++) {
            reference_crc = static_cast<U32>(update_crc_32(reference_crc, static_cast<CHAR>(this->m_data[i])));
        }
        offset += static_cast<U32>(chunk);
    }
    if (reference_crc != header.reference_crc32) {
        flash_area_close(area);
        this->log_WARNING_HI_PatchReferenceMismatch(header.reference_crc32, header.reference_size, reference_crc);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // A compressed patch is decoded once into a scratch file, so the apply always works on plain
    // streams and needs no codec of its own
    Fw::String stream_source(patch);
    Fw::String scratch_path(destination);
    scratch_path += ".streams";
    const U32 stream_bytes = header.control_size + header.diff_size + header.extra_size;
    FwSizeType stream_base = static_cast<FwSizeType>(PatchApplier::HEADER_SIZE);
    if (header.compression != PatchApplier::Compression::NONE) {
        if (!this->decompressPatch(patch, scratch_path.toChar(), stream_bytes)) {
            flash_area_close(area);
            this->log_WARNING_HI_PatchFailed(static_cast<U8>(PatchApplier::Error::PATCH_READ_FAILED));
            this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
            return;
        }
        stream_source = scratch_path;
        stream_base = 0;
    }

    // Three cursors into one file, one per stream, so the apply can interleave them
    Os::File control_file;
    Os::File diff_file;
    Os::File extra_file;
    Os::File out_file;
    const FwSizeType control_start = stream_base;
    const FwSizeType diff_start = control_start + static_cast<FwSizeType>(header.control_size);
    const FwSizeType extra_start = diff_start + static_cast<FwSizeType>(header.diff_size);
    const bool opened =
        (control_file.open(stream_source.toChar(), Os::File::Mode::OPEN_READ) == Os::File::Status::OP_OK) &&
        (diff_file.open(stream_source.toChar(), Os::File::Mode::OPEN_READ) == Os::File::Status::OP_OK) &&
        (extra_file.open(stream_source.toChar(), Os::File::Mode::OPEN_READ) == Os::File::Status::OP_OK) &&
        (out_file.open(destination.toChar(), Os::File::Mode::OPEN_CREATE, Os::File::OverwriteType::OVERWRITE) ==
         Os::File::Status::OP_OK) &&
        (control_file.seek(control_start, Os::File::SeekType::ABSOLUTE) == Os::File::Status::OP_OK) &&
        (diff_file.seek(diff_start, Os::File::SeekType::ABSOLUTE) == Os::File::Status::OP_OK) &&
        (extra_file.seek(extra_start, Os::File::SeekType::ABSOLUTE) == Os::File::Status::OP_OK);
    if (!opened) {
        flash_area_close(area);
        this->log_WARNING_HI_PatchFailed(static_cast<U8>(PatchApplier::Error::PATCH_READ_FAILED));
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    PatchIo io(area, control_file, diff_file, extra_file, out_file);
    error = PatchApplier::apply(header, header.reference_size, io, this->m_data, sizeof(this->m_data));
    flash_area_close(area);
    out_file.close();
    if (error != PatchApplier::Error::NONE) {
        this->log_WARNING_HI_PatchFailed(static_cast<U8>(error));
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // Independent check that the reconstruction is what the ground intended
    Os::File verify;
    U32 actual_crc = 0;
    if ((verify.open(destination.toChar(), Os::File::Mode::OPEN_READ) != Os::File::Status::OP_OK) ||
        (verify.calculateCrc(actual_crc) != Os::File::Status::OP_OK)) {
        this->log_WARNING_HI_PatchFailed(static_cast<U8>(PatchApplier::Error::OUTPUT_WRITE_FAILED));
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    if (actual_crc != crc32) {
        this->log_WARNING_LO_ImageFileCrcMismatch(destination, Os::FileStatus(Os::FileStatus::T::OP_OK), crc32,
                                                  actual_crc);
        this->log_WARNING_HI_PatchFailed(static_cast<U8>(PatchApplier::Error::SIZE_MISMATCH));
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    this->log_ACTIVITY_HI_PatchSucceeded(header.new_size);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
