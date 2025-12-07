# UpdateStatus/UpdateStatus.fpp:
#
# File provided by the UpdateWorker component to report status codes back to the Updater component. This is configured
# specifically for the FlashWorker implementation supplied by Proves.
#
# Copyright (c) 2025 Michael Starch
#
# Licensed under the Apache License, Version 2.0. See LICENSE for details.
#
module Update {
    @ Status codes used by the Update subsystem.
    enum FlashWorkerUpdateStatus {
        OP_OK, @< REQUIRED: operation successful
        BUSY, @< REQUIRED: operation could not be started because another operation is in-progress
        UNPREPARED, @< Preparation step was not completed and not optional
        PREPARATION_ERROR, @< An error occurred during the preparation step
        IMAGE_FILE_READ_ERROR, @< An error occurred reading the image file
        IMAGE_CRC_MISMATCH, @< The image file failed CRC validation
        NEXT_BOOT_ERROR @< An error occurred setting the next boot image
    }
}
