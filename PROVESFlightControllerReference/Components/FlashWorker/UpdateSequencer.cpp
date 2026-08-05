// ======================================================================
// \title  UpdateSequencer.cpp
// \brief  cpp file for flight software update sequencing helper class
// ======================================================================

#include "UpdateSequencer.hpp"

namespace Components {

UpdateSequencer ::UpdateSequencer() : m_step(Step::IDLE), m_last_failed(false) {}

UpdateSequencer ::~UpdateSequencer() {}

UpdateSequencer::Step UpdateSequencer ::step() const {
    return this->m_step;
}

bool UpdateSequencer ::isPrepared() const {
    return this->m_step == Step::PREPARED;
}

bool UpdateSequencer ::dirtiesStagingSlot(WriteOutcome outcome) {
    switch (outcome) {
        // These fail before the first byte reaches the flash, so the erased slot is still good
        case WriteOutcome::FILE_OPEN_FAILED:
        case WriteOutcome::FILE_QUERY_FAILED:
        case WriteOutcome::CRC_MISMATCH:
            return false;
        // These fail part way through, leaving a partial image behind
        case WriteOutcome::FILE_READ_FAILED:
        case WriteOutcome::FLASH_WRITE_FAILED:
            return true;
        case WriteOutcome::SUCCESS:
        default:
            return false;
    }
}

UpdateSequencer::Status UpdateSequencer ::statusForOutcome(WriteOutcome outcome) {
    switch (outcome) {
        case WriteOutcome::SUCCESS:
            return Status::OP_OK;
        case WriteOutcome::FILE_OPEN_FAILED:
        case WriteOutcome::FILE_QUERY_FAILED:
        case WriteOutcome::FILE_READ_FAILED:
            return Status::IMAGE_FILE_READ_ERROR;
        case WriteOutcome::CRC_MISMATCH:
            return Status::IMAGE_CRC_MISMATCH;
        case WriteOutcome::FLASH_WRITE_FAILED:
            return Status::FLASH_WRITE_ERROR;
        default:
            return Status::FLASH_WRITE_ERROR;
    }
}

UpdateSequencer::Status UpdateSequencer ::onPrepareComplete(bool erase_succeeded) {
    this->m_last_failed = !erase_succeeded;
    if (!erase_succeeded) {
        // A failed erase leaves the slot in an unknown state; require another preparation
        this->m_step = Step::IDLE;
        return Status::PREPARATION_ERROR;
    }
    this->m_step = Step::PREPARED;
    return Status::OP_OK;
}

UpdateSequencer::Status UpdateSequencer ::onUpdateComplete(WriteOutcome outcome) {
    // An update may only follow a successful preparation. A rejected attempt does no work, so it
    // leaves the sequence untouched rather than forcing an unnecessary erase.
    if (!this->isPrepared()) {
        return Status::UNPREPARED;
    }
    this->m_last_failed = (outcome != WriteOutcome::SUCCESS);
    if (outcome == WriteOutcome::SUCCESS) {
        this->m_step = Step::UPDATED;
        return Status::OP_OK;
    }
    // Only fall back to IDLE when the slot actually holds partial data. Otherwise stay PREPARED so
    // that a mistyped file name or a bad CRC can be retried without paying for another erase.
    if (UpdateSequencer::dirtiesStagingSlot(outcome)) {
        this->m_step = Step::IDLE;
    }
    return UpdateSequencer::statusForOutcome(outcome);
}

UpdateSequencer::Stage UpdateSequencer ::settledStage() const {
    if (this->m_last_failed) {
        return Stage::FAILED;
    }
    switch (this->m_step) {
        case Step::PREPARED:
            return Stage::PREPARED;
        case Step::UPDATED:
            return Stage::UPDATED;
        case Step::IDLE:
        default:
            return Stage::IDLE;
    }
}

uint8_t UpdateSequencer ::percentComplete(uint32_t written, uint32_t total) {
    if (total == 0) {
        return 0;
    }
    if (written >= total) {
        return 100;
    }
    // 64-bit intermediate: a 32-bit multiply would overflow well below the 1 MB slot size
    const uint64_t percent = (static_cast<uint64_t>(written) * 100U) / total;
    return static_cast<uint8_t>(percent);
}

bool UpdateSequencer ::progressReportDue(uint8_t percent, uint8_t last_reported, uint8_t step_percent) {
    // Completion is always worth reporting
    if (percent >= 100) {
        return last_reported < 100;
    }
    // Never let a misconfigured step silence reporting entirely
    const uint8_t step = (step_percent == 0) ? 1 : step_percent;
    if (percent < last_reported) {
        return false;
    }
    return static_cast<uint8_t>(percent - last_reported) >= step;
}

}  // namespace Components
