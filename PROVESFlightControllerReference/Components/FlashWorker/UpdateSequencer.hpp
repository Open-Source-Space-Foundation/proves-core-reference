// ======================================================================
// \title  UpdateSequencer.hpp
// \brief  hpp file for flight software update sequencing helper class
// ======================================================================

#pragma once

#include <cstdint>

namespace Components {

//! Tracks the ordering of the flight software update steps and decides the status reported
//! back to the Updater component for a given outcome.
//!
//! This logic is kept free of F Prime and Zephyr dependencies so that it can be exercised by
//! host unit tests. Components::FlashWorker owns an instance and performs the actual platform
//! work; the sequencer only decides "what does this outcome mean" and "what may happen next".
class UpdateSequencer {
  public:
    // ----------------------------------------------------------------------
    //  Public types
    // ----------------------------------------------------------------------

    //! Status codes reported back to the Updater component.
    //!
    //! Mirrors Update.FlashWorkerUpdateStatus in UpdateStatus/UpdateStatus.fpp. FlashWorker.cpp
    //! static_asserts that the two agree, so reordering either one is a compile error.
    enum class Status : uint8_t {
        OP_OK = 0,                  //!< Operation successful
        BUSY = 1,                   //!< Another operation is in progress
        UNPREPARED = 2,             //!< Preparation step was not completed
        PREPARATION_ERROR = 3,      //!< An error occurred during the preparation step
        IMAGE_FILE_READ_ERROR = 4,  //!< An error occurred reading the image file
        IMAGE_CRC_MISMATCH = 5,     //!< The image file failed CRC validation
        NEXT_BOOT_ERROR = 6,        //!< An error occurred setting the next boot image
        FLASH_WRITE_ERROR = 7,      //!< An error occurred writing the image to the staging slot
    };

    //! Steps of the update sequence that have completed successfully
    enum class Step : uint8_t {
        IDLE = 0,      //!< No usable staging slot; PREPARE_UPDATE must run before an update
        PREPARED = 1,  //!< Staging slot erased and ready to receive an image
        UPDATED = 2,   //!< An image has been written to the staging slot
    };

    //! Stage of the update sequence reported as telemetry.
    //!
    //! Mirrors Components.FlashUpdateStage in FlashWorker.fpp. FlashWorker.cpp static_asserts that
    //! the two agree. PREPARING and WRITING are transient and are reported by FlashWorker while an
    //! operation is running; the sequencer only knows the settled stages.
    enum class Stage : uint8_t {
        IDLE = 0,       //!< No update in progress
        PREPARING = 1,  //!< Erasing the staging slot
        PREPARED = 2,   //!< Staging slot erased and ready to receive an image
        WRITING = 3,    //!< Writing an image into the staging slot
        UPDATED = 4,    //!< Image written and verified in the staging slot
        FAILED = 5,     //!< The last operation failed
    };

    //! Outcome of an attempt to write an image into the staging slot
    enum class WriteOutcome : uint8_t {
        SUCCESS = 0,             //!< The whole image was written
        FILE_OPEN_FAILED = 1,    //!< The image file could not be opened
        FILE_QUERY_FAILED = 2,   //!< Sizing, CRC, or seek of the image file failed before any write
        CRC_MISMATCH = 3,        //!< The image file failed CRC validation before any write
        FILE_READ_FAILED = 4,    //!< Reading the image file failed part way through the write
        FLASH_WRITE_FAILED = 5,  //!< Writing to the staging slot failed
    };

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct UpdateSequencer object
    UpdateSequencer();

    //! Destroy UpdateSequencer object
    ~UpdateSequencer();

  public:
    // ----------------------------------------------------------------------
    //  Public helper methods
    // ----------------------------------------------------------------------

    //! Last update step to have completed successfully
    Step step() const;

    //! Whether the staging slot is erased and an image may be written to it
    bool isPrepared() const;

    //! Whether an outcome left partially written data in the staging slot.
    //!
    //! An outcome that never reached the flash leaves the slot erased and still usable, so the
    //! operator may retry the update directly. An outcome that did reach the flash requires
    //! another erase, which costs a full slot erase on orbit.
    static bool dirtiesStagingSlot(WriteOutcome outcome);

    //! Status reported to the Updater component for a write outcome
    static Status statusForOutcome(WriteOutcome outcome);

    //! Record the result of the preparation (staging slot erase) step
    Status onPrepareComplete(bool erase_succeeded);

    //! Record the result of an image write and advance or reset the sequence accordingly
    Status onUpdateComplete(WriteOutcome outcome);

    //! Settled stage of the sequence, for telemetry
    Stage settledStage() const;

    //! Percentage of an image written so far.
    //!
    //! Saturates at 100 and reports 0 for an empty image, so that a bad or missing size can never
    //! produce a divide by zero or a nonsense percentage in telemetry.
    static uint8_t percentComplete(uint32_t written, uint32_t total);

    //! Whether a progress report is due.
    //!
    //! Reports every step_percent of progress, and always reports completion. A step of 0 is
    //! treated as 1 so that a misconfigured parameter cannot disable reporting entirely.
    static bool progressReportDue(uint8_t percent, uint8_t last_reported, uint8_t step_percent);

  private:
    // ----------------------------------------------------------------------
    //  Private member variables
    // ----------------------------------------------------------------------

    Step m_step;         //!< Last step to have completed successfully
    bool m_last_failed;  //!< Whether the most recent operation reported a failure
};

}  // namespace Components
