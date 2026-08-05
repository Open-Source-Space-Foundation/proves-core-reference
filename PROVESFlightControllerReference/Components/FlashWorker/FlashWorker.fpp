module Components {
    @ Stage of the flight software update sequence, reported as telemetry so that an operator can
    @ see where an update stands without replaying the event history from an earlier pass.
    enum FlashUpdateStage {
        IDLE, @< No update in progress
        PREPARING, @< Erasing the staging slot
        PREPARED, @< Staging slot erased and ready to receive an image
        WRITING, @< Writing an image into the staging slot
        UPDATED, @< Image written and verified in the staging slot
        FAILED @< The last operation failed; see LastUpdateStatus
    }

    @ Performs long-running operations for the flash subsystem
    active component FlashWorker {
        import Update.UpdateWorker

        @ Microseconds to pause after each buffered flash write, giving the flash time to settle.
        @ Exposed as a parameter so that it can be tuned against real hardware rather than rebuilt.
        param CHUNK_DELAY_US: U32 default 5000

        @ Percent of the image to advance between progress reports. Larger values mean fewer
        @ progress events and less downlink spent reporting on an update in flight.
        param PROGRESS_STEP_PERCENT: U8 default 10

        @ Whether the flight software may confirm a test-booted image without the ground.
        @
        @ An image booted in TEST mode reverts unless it is confirmed before the next reboot. If the
        @ confirming pass is missed, a working image is thrown away along with the uplink that
        @ delivered it. Arming this lets the spacecraft keep an image that has demonstrated it can
        @ run. Defaults to disabled, so confirmation stays operator-in-the-loop until armed.
        @
        @ Set with AUTO_CONFIRM_ENABLED_PRM_SET followed by PRM_SAVE: the decision is made after the
        @ reboot into the test image, so an unsaved value would be lost exactly when it is needed.
        param AUTO_CONFIRM_ENABLED: bool default false

        @ Seconds the test-booted image must run continuously before it confirms itself. Counted
        @ from the start of the image that is pending confirmation, and reset by any reboot.
        param AUTO_CONFIRM_DELAY_SECONDS: U32 default 1800

        @ Concatenate numbered uplink segments into a single image file.
        @
        @ A full image does not fit in one pass and file uplink cannot resume across passes, so an
        @ image is uplinked as "<prefix>.000", "<prefix>.001", and so on. This joins them back
        @ together and validates the result before it is written to flash.
        async command ASSEMBLE_IMAGE(
            prefix: string size FileNameStringSize @< Segment file name prefix
            segments: U16 @< Number of segments to join
            destination: string size FileNameStringSize @< Image file to write
            crc32: U32 @< Expected CRC32 of the assembled image
        )

        @ Reconstruct an image from a delta patch applied to the running image.
        @
        @ Uplinking a patch rather than a whole image is the difference between an update that fits
        @ a pass and one that does not. The patch names the reference it was built from and is
        @ refused if the running image is not it.
        async command APPLY_PATCH(
            patch: string size FileNameStringSize @< Patch file to apply
            destination: string size FileNameStringSize @< Image file to write
            crc32: U32 @< Expected CRC32 of the reconstructed image
        )

        @ Stage of the update sequence
        telemetry UpdateStage: FlashUpdateStage

        @ Bytes of the image written into the staging slot so far
        telemetry BytesWritten: U32

        @ Total size in bytes of the image currently being written
        telemetry ImageTotalBytes: U32

        @ Status reported by the most recent preparation or update operation
        telemetry LastUpdateStatus: Update.UpdateStatus

        @ Whether the running image has been confirmed. False means this is a test boot that will
        @ revert on the next reboot unless it is confirmed.
        telemetry RunningImageConfirmed: bool

        @ Seconds the running image has been up while pending confirmation
        telemetry PendingConfirmSeconds: U32

        @ Scheduled input used to age a test-booted image toward self-confirmation
        sync input port run: Svc.Sched

        event UpdateProgress(written: U32, total: U32, percent: U8) severity activity low \
            format "Update progress: {}/{} bytes ({}%)"

        event NoImagePrepared() severity warning low \
            format "No image has been prepared for update"

        event NextBootSetFailed(mode: Update.NextBootMode, error_number: I32) severity warning low \
            format "Set next boot mode to {} failed (errno: {})"

        event ConfirmImageFailed(error_number: I32) severity warning low \
            format "Confirm image failed (errno: {})"

        event FlashEraseFailed(error_number: I32) severity warning low \
            format "Flash erase failed (errno: {})"

        event FlashWriteFailed(error_number: I32, bytes: FwSizeType) severity warning low \
            format "Flash write failed (errno: {}) at {}"

        event ImageFileReadError(file_name: string, error: Os.FileStatus) severity warning low \
            format "Failed to read {} with error {}"

        event ImageFileCrcMismatch(file_name: string, error: Os.FileStatus, expected: U32, actual: U32) \
            severity warning low \
            format "Failed CRC validation of {} with status {} expected 0x{x} and actual 0x{x}"

        @ Emitted when the bytes written to flash do not match the bytes that were validated. This
        @ indicates the image file changed underneath the update or the filesystem read is unstable.
        event ImageWriteCrcMismatch(expected: U32, actual: U32) severity warning high \
            format "Image written to flash failed verification: expected 0x{x} and actual 0x{x}"

        @ The running image confirmed itself after demonstrating it can run
        event AutoConfirmed(seconds: U32) severity activity high \
            format "Test image confirmed automatically after {} seconds of operation"

        event AutoConfirmFailed(error_number: I32) severity warning high \
            format "Automatic confirmation failed (errno: {})"

        event AssembleStarted(segments: U16, destination: string) severity activity high \
            format "Assembling {} segments into {}"

        event AssembleSucceeded(bytes: U32) severity activity high \
            format "Assembled image of {} bytes"

        event AssembleFailed(segment: U16, status: Update.UpdateStatus) severity warning high \
            format "Assembly failed at segment {} with status {}"

        event InvalidSegmentCount(segments: U16) severity warning high \
            format "Segment count {} is out of range"

        event PatchStarted(patch: string, destination: string) severity activity high \
            format "Applying patch {} to produce {}"

        event PatchSucceeded(bytes: U32) severity activity high \
            format "Reconstructed image of {} bytes from patch"

        @ The error code is Components::PatchApplier::Error
        event PatchFailed(error_code: U8) severity warning high \
            format "Patch application failed with error {}"

        @ Emitted when the running image is not the one the patch was built against. Applying the
        @ patch anyway would produce a corrupt image, so it is refused.
        event PatchReferenceMismatch(expected_crc: U32, expected_size: U32, actual_crc: U32) \
            severity warning high \
            format "Patch reference mismatch: expected CRC 0x{x} over {} bytes but running image has CRC 0x{x}"

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port to return the value of a parameter
        param get port prmGetOut

        @ Port to set the value of a parameter
        param set port prmSetOut

        @ Port for sending command registrations
        command reg port cmdRegOut

        @ Port for receiving commands
        command recv port cmdIn

        @ Port for sending command responses
        command resp port cmdResponseOut

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

        @ Port for sending telemetry channels to downlink
        telemetry port tlmOut

    }
}
