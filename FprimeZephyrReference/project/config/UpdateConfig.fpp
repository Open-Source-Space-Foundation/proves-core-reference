# Update/Subtopology/UpdateConfig/UpdateConfig.fpp:
#
# Update Subtopology configuration
#
# Copyright (c) 2025 Michael Starch
#
# Licensed under the Apache License, Version 2.0. See LICENSE for details.
#
module Update {
    # Base ID for instance IDs in this subtopology
    constant BASE_ID = 0xA0000000

    module QueueSizes {
        constant updater = 5 # Updater should process quickly; small queue
        constant worker  = 1 # Worker only handles one item at a time
    }

    module StackSizes {
        constant updater = 8 * 1024 # Minimal stack size, not much processing
        constant worker  = 8 * 1024 # Minimal stack size, not much processing
    }

    module Priorities {
        constant updater = 5 # Updater does real-time tasks; higher priority
        constant worker  = 15 # Worker does slow-running tasks; lower priority
    }

    instance worker: Components.FlashWorker base id BASE_ID + 0x1000 \
        queue size QueueSizes.worker \
        stack size StackSizes.worker \
        priority Priorities.worker
}
