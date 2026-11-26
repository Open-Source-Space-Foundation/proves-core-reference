module Components {

    @ Where an expansion is (mirrors Upz::Phase)
    enum ExpandPhase: U8 {
        IDLE = 0
        HEADER = 1
        RESUME = 2
        DECODE = 3
        FINAL = 4
        DONE = 5
        FAILED = 6
    }

    @ Outcome of an expansion (mirrors Upz::Status)
    enum ExpandStatus: U8 {
        OK = 0
        IO_ERROR = 1
        BAD_HEADER = 2
        BAD_HEADER_CRC = 3
        UNSUPPORTED = 4
        CORRUPT_STREAM = 5
        CHUNK_CRC = 6
        FINAL_CRC = 7
        BUSY = 8
        BAD_PATH = 9
    }

    @ Rebuilds a file uplinked in UPZ form (tools/upz/upz.py): Thumb-2 branch filter + chunked raw LZMA.
    @ Work runs in bounded steps from `run` (connect a slow rate group), so a long expansion spreads over the idle
    @ time between passes. RAM is fixed (about 12 KB); the LZMA window is read back from the output file, so it costs
    @ storage reads, not RAM. Progress is derived from the output file and the per-chunk CRC table, so after a reset
    @ resending the same EXPAND_FILE resumes at the first chunk that is missing or wrong.
    queued component FileExpander {

        @ Rate-group tick: dispatches queued commands, then does one bounded step
        guarded input port run: Svc.Sched

        @ Start expanding coded_file into out_file. Acknowledged at once; completion is the ExpandComplete or
        @ ExpandFailed event. Resumes if out_file already holds verified chunks.
        async command EXPAND_FILE(
            coded_file: string size FW_CMD_STRING_MAX_SIZE @< Uplinked .upz file
            out_file: string size FW_CMD_STRING_MAX_SIZE @< File to rebuild (created or resumed)
        )

        @ Stop the expansion in progress; the partial output is kept for a later resume
        async command ABORT_EXPAND()

        @ An expansion was accepted
        event ExpandStarted(
            coded_file: string size FileNameStringSize
            out_file: string size FileNameStringSize
        ) severity activity high format "Expanding {} -> {}"

        @ The header was valid; decoding starts at this chunk (non-zero means earlier chunks were kept)
        event ExpandResumed(
            chunk: U32 @< First chunk decoded
            total: U32 @< Chunks in the file
            raw_size: U32 @< Size of the rebuilt file
        ) severity activity high format "Decoding from chunk {} of {} ({} bytes)"

        @ The output was rebuilt and re-read from storage with the expected size and CRC32
        event ExpandComplete(
            out_file: string size FileNameStringSize
            raw_size: U32 @< Bytes written
            crc32: U32 @< zlib CRC32 of the output
            far_reads: U32 @< Window blocks read back from the output
            ticks: U32 @< Rate-group steps used
        ) severity activity high format "Expanded {} ({} bytes, crc {x}) with {} read-backs in {} ticks"

        @ The expansion stopped; the partial output is kept
        event ExpandFailed(
            status: ExpandStatus @< Reason
            chunk: U32 @< Chunk being processed
        ) severity warning high format "Expansion failed: {} at chunk {}"

        @ The operator stopped the expansion
        event ExpandAborted(
            chunk: U32 @< Chunk that was being processed
        ) severity activity high format "Expansion aborted at chunk {}"

        @ Current phase
        telemetry Phase: ExpandPhase update on change

        @ Chunk being processed
        telemetry Chunk: U32 update on change

        @ Output bytes decoded so far in the current job
        telemetry BytesDone: U64 update on change

        @ Outcome of the last expansion
        telemetry LastStatus: ExpandStatus update on change

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

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
