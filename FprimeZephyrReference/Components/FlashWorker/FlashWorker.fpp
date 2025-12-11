module Components {
    @ Performs long-running operations for the flash subsystem
    active component FlashWorker {
        import Update.UpdateWorker

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

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

    }
}
