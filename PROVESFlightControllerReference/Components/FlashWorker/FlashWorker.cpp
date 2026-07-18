// ======================================================================
// \title  FlashWorker.cpp
// \author starchmd
// \brief  cpp file for FlashWorker component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/FlashWorker/FlashWorker.hpp"

#include "Os/File.hpp"
#include "Os/Task.hpp"
#include <zephyr/dfu/flash_img.h>
#include <zephyr/dfu/mcuboot.h>

namespace Components {
// static_assert(FlashWorker::REGION_NUMBER == UPLOAD_FLASH_AREA_LABEL,
//     "FlashWorker REGION_NUMBER must match zephyr mcuboot image for upload area");

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

FlashWorker ::FlashWorker(const char* const compName) : FlashWorkerComponentBase(compName), m_last_successful(IDLE) {}

FlashWorker ::~FlashWorker() {}

// ----------------------------------------------------------------------
// Flash helpers
// ----------------------------------------------------------------------

Update::UpdateStatus FlashWorker ::writeImage(const Fw::StringBase& file_name, Os::File& file, U32 expected_crc32) {
    const FwSizeType CHUNK = static_cast<FwSizeType>(sizeof(this->m_data));
    FW_ASSERT(file.isOpen());
    FwSizeType size = 0;
    U32 file_crc = 0;
    Os::File::Status file_status = file.size(size);
    if (file_status != Os::File::Status::OP_OK) {
        this->log_WARNING_LO_ImageFileReadError(file_name, Os::FileStatus(static_cast<Os::FileStatus::T>(file_status)));
        return Update::UpdateStatus::IMAGE_FILE_READ_ERROR;
    }
    // Validate the CRC before touching flash: a corrupted upload must never reach the slot
    file_status = file.calculateCrc(file_crc);
    if (file_status != Os::File::Status::OP_OK || file_crc != expected_crc32) {
        this->log_WARNING_LO_ImageFileCrcMismatch(
            file_name, Os::FileStatus(static_cast<Os::FileStatus::T>(file_status)), expected_crc32, file_crc);
        return Update::UpdateStatus::IMAGE_CRC_MISMATCH;
    }
    file_status = file.seek(0, Os::File::SeekType::ABSOLUTE);
    if (file_status != Os::File::Status::OP_OK) {
        this->log_WARNING_LO_ImageFileReadError(file_name, Os::FileStatus(static_cast<Os::FileStatus::T>(file_status)));
        return Update::UpdateStatus::IMAGE_FILE_READ_ERROR;
    }
    int status = flash_img_init_id(&this->m_flash_context, FlashWorker::REGION_NUMBER);
    if (status != 0) {
        this->log_WARNING_LO_FlashWriteFailed(static_cast<I32>(-1 * status), 0);
        return Update::UpdateStatus::FLASH_WRITE_ERROR;
    }
    for (FwSizeType i = 0; i < size; i += CHUNK) {
        FwSizeType read_size = CHUNK;
        file_status = file.read(this->m_data, read_size);
        if (file_status != Os::File::Status::OP_OK) {
            this->log_WARNING_LO_ImageFileReadError(file_name,
                                                    Os::FileStatus(static_cast<Os::FileStatus::T>(file_status)));
            return Update::UpdateStatus::IMAGE_FILE_READ_ERROR;
        }
        status = flash_img_buffered_write(&this->m_flash_context, this->m_data, read_size, true);
        if (status != 0) {
            this->log_WARNING_LO_FlashWriteFailed(static_cast<I32>(-1 * status), i);
            return Update::UpdateStatus::FLASH_WRITE_ERROR;
        }
        // Give 5ms for flash to process data and allow data to be loaded off the flash
        Os::Task::delay(Fw::TimeInterval(0, 5000));
    }
    return Update::UpdateStatus::OP_OK;
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
    Update::UpdateStatus return_status = Update::UpdateStatus::OP_OK;
    int status = boot_erase_img_bank(FlashWorker::REGION_NUMBER);
    if (status != 0) {
        this->log_WARNING_LO_FlashEraseFailed(static_cast<I32>(-1 * status));
        return_status = Update::UpdateStatus::PREPARATION_ERROR;
    } else {
        this->m_last_successful = PREPARE;
    }
    this->prepareImageDone_out(0, return_status);
}

void FlashWorker ::updateImage_handler(FwIndexType portNum, const Fw::StringBase& file, U32 crc32) {
    Os::File image_file;
    Update::UpdateStatus return_status = Update::UpdateStatus::OP_OK;

    if (this->m_last_successful != PREPARE) {
        return_status = Update::UpdateStatus::UNPREPARED;
        this->m_last_successful = IDLE;
        this->log_WARNING_LO_NoImagePrepared();
    } else {
        Os::File::Status file_status = image_file.open(file.toChar(), Os::File::Mode::OPEN_READ);
        if (file_status == Os::File::Status::OP_OK) {
            return_status = this->writeImage(file, image_file, crc32);
            if (return_status == Update::UpdateStatus::OP_OK) {
                this->m_last_successful = UPDATE;
            } else if (return_status == Update::UpdateStatus::IMAGE_CRC_MISMATCH) {
                // Flash was not touched: the slot is still erased, so the operator may re-uplink
                // the file and retry the update without another PREPARE_UPDATE
                this->m_last_successful = PREPARE;
            } else {
                // Flash may hold a partial image: require a fresh PREPARE_UPDATE (erase) before retrying
                this->m_last_successful = IDLE;
            }
        } else {
            return_status = Update::UpdateStatus::IMAGE_FILE_READ_ERROR;
            this->m_last_successful = IDLE;
            this->log_WARNING_LO_ImageFileReadError(file, static_cast<Os::FileStatus::T>(file_status));
        }
    }
    this->updateImageDone_out(0, return_status);
}

}  // namespace Components
