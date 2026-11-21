module Components {
    @ Monitor the RP2350 QMI PSRAM region (APS1604M, issue #285 S6)
    passive component PsramMonitor {

        # ------------------------------------------------------------------
        # Telemetry
        # ------------------------------------------------------------------

        @ Total PSRAM region size in bytes (0 when absent/disabled)
        telemetry PsramSize: U32

        @ PSRAM status: 0=absent/init-failed, 1=mapped, 2=heap-verified
        telemetry PsramStatus: U8

        # ------------------------------------------------------------------
        # Commands
        # ------------------------------------------------------------------

        @ Bounded memtest over [base+start_offset, +length) through the
        @ uncached alias.  Length is clamped to 256 KB per invocation.
        @ Rejected (with event) when PSRAM is not ready.
        sync command PSRAM_SELF_TEST(
            start_offset: U32  @< Byte offset from PSRAM base
            length: U32        @< Number of bytes to test (clamped to 256 KB)
        )

        # ------------------------------------------------------------------
        # Events
        # ------------------------------------------------------------------

        @ PSRAM self-test passed
        event PsramSelfTestPassed(
            start_offset: U32  @< Start offset tested
            length: U32        @< Length tested
        ) \
            severity activity low \
            format "PSRAM self-test PASSED: offset={} length={} bytes"

        @ PSRAM self-test failed — first mismatch location
        event PsramSelfTestFailed(
            offset: U32     @< Byte offset of first mismatch
            expected: U32   @< Expected value
            actual: U32     @< Actual value read back
        ) \
            severity warning high \
            format "PSRAM self-test FAILED at offset={}: expected=0x{x} actual=0x{x}"

        @ PSRAM self-test rejected because PSRAM is not ready
        event PsramNotReady() \
            severity warning low \
            format "PSRAM_SELF_TEST rejected: PSRAM not ready"

        # ------------------------------------------------------------------
        # Ports
        # ------------------------------------------------------------------

        @ Rate-group driven run port (1 Hz)
        sync input port run: Svc.Sched

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
