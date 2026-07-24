"""
conftest.py:

Pytest configuration for integration tests.
"""

import csv
import os
import threading
import time

import pytest
from common import cmdDispatch, resync_sequence_number, set_radio_recover_fn
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

# After TRANSMIT is first enabled the satellite flushes the event backlog that
# accumulated during boot (while transmit was DISABLED).  Wait this many seconds
# for the burst to subside before clearing histories and starting tests, so that
# command-ack events are not lost in the initial flood.
RADIO_STABILIZE_S = 15


# Hardware that a bare flight control board does not have. When
# --bare-flight-controler-board is passed, tests carrying any of these markers are
# skipped so the suite can run on the board alone.
BARE_FCB_SKIP_MARKERS = {
    "requires_face": "a face board",
    "requires_antenna": "the antenna board and burnwire capacitor",
    "requires_battery": "the battery board with power at the terminals",
    "requires_watchdog_jumper": "the JP6 watchdog jumper bridged",
}


def pytest_collection_modifyitems(
    config: pytest.Config, items: list[pytest.Item]
) -> None:
    """Adjust the collected tests for the current hardware/link configuration.

    1. With --bare-flight-controller-board, skip tests whose markers require add-on
       hardware (face, antenna, battery, or the JP6 watchdog jumper).
    2. With --with-radio, move the RTC tests to the end of the collection. The
       RTC tests mutate the system clock (TIME_SET to -12 h from now) and the
       teardown resets it to current UTC.  Running them last prevents the
       temporary clock change from racing with the test that immediately follows
       alphabetically (TMP112), which is otherwise flaky on the radio path.
    """
    if config.getoption("--bare-flight-controler-board", default=False):
        for item in items:
            needed = [
                description
                for marker, description in BARE_FCB_SKIP_MARKERS.items()
                if item.get_closest_marker(marker) is not None
            ]
            if needed:
                item.add_marker(
                    pytest.mark.skip(
                        reason="bare flight control board: requires "
                        + "; ".join(needed)
                    )
                )

    if not config.getoption("--with-radio", default=False):
        return

    rtc_items = [i for i in items if "rtc_test" in i.nodeid]
    non_rtc_items = [i for i in items if "rtc_test" not in i.nodeid]
    items[:] = non_rtc_items + rtc_items


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--with-radio",
        action="store_true",
        default=False,
        help="Enable radio setup and teardown fixtures for this test run.",
    )
    parser.addoption(
        "--sync-deframer",
        choices=["uart", "lora"],
        default=None,
        help="Which TcSecurityDeframer instance the sequence-number sync test "
        "queries. Defaults to lora when --with-radio is set, uart otherwise. "
        "The radio CI job's UART-side bootstrap passes --sync-deframer=lora "
        "because the traffic that follows is validated by the LoRa instance.",
    )
    parser.addoption(
        "--command-retries",
        type=int,
        default=None,
        help="Override retry count for proves_send_and_assert_command (default: 3 UART, 5 radio).",
    )
    parser.addoption(
        "--bare-flight-controler-board",
        action="store_true",
        default=False,
        help="Skip tests that require add-on hardware (face, antenna, battery, or "
        "the JP6 watchdog jumper) so the suite can run on a bare flight control board.",
    )


def pytest_configure(config: pytest.Config) -> None:
    explicit = config.getoption("--command-retries", default=None)
    if explicit is not None:
        from common import set_default_retries

        set_default_retries(explicit)
    elif config.getoption("--with-radio", default=False):
        from common import set_default_retries

        set_default_retries(5)


@pytest.fixture(scope="session")
def start_gds(
    request: pytest.FixtureRequest, fprime_test_api_session: IntegrationTestAPI
):
    """Fixture to start GDS before tests and stop after tests

    GDS is used to send commands and receive telemetry/events.
    """
    gds_working = False
    timeout_time = time.time() + 30
    while time.time() < timeout_time:
        try:
            if request.config.getoption("--with-radio"):
                _enable_radio(fprime_test_api_session)
            fprime_test_api_session.send_and_assert_command(
                command=f"{cmdDispatch}.CMD_NO_OP"
            )
            gds_working = True
            break
        except Exception:
            time.sleep(1)
    assert gds_working

    if request.config.getoption("--with-radio"):
        # Allow the boot-time event backlog to drain before any test commands
        # are issued.  Without this wait the initial burst of queued events can
        # swamp command-ack events and cause the first test assertions to fail.
        time.sleep(RADIO_STABILIZE_S)
        fprime_test_api_session.clear_histories()

    yield


def _enable_radio(fprime_test_api: IntegrationTestAPI) -> None:
    # Divider must be set while TRANSMIT is DISABLED — radio parameter changes are latched on enable.
    # Setting it after enable leaves the default divider (300) active, producing a ~30s downlink gap
    # that exceeds the test sequence-search timeout.
    # Fire-and-forget (no per-call retry): start_gds wraps this in its own 30s
    # retry budget around a follow-up NO_OP liveness check. Wrapping each send
    # in proves_send_and_assert_command (3x10s) consumes that whole budget on
    # the first attempt and prevents start_gds from ever retrying.
    fprime_test_api.send_command(
        command="ReferenceDeployment.downlinkDelay.DIVIDER_PRM_SET",
        args=[20],
    )
    fprime_test_api.send_command(
        command="ReferenceDeployment.lora.TRANSMIT", args=["ENABLED"]
    )


@pytest.fixture(autouse=True)
def start_radio(request: pytest.FixtureRequest, fprime_test_api: IntegrationTestAPI):
    """Fixture to start the radio before tests"""
    if not request.config.getoption("--with-radio"):
        return

    _enable_radio(fprime_test_api)

    # Register the link recovery callback so that proves_send_and_assert_command
    # can re-enable TRANSMIT after RADIO_RECOVER_THRESHOLD consecutive failures.
    # The lambda captures fprime_test_api for this test's function scope.
    set_radio_recover_fn(lambda: _enable_radio(fprime_test_api))


@pytest.fixture(autouse=True)
def resync_sequence_number_after_reboot(
    request: pytest.FixtureRequest,
    fprime_test_api: IntegrationTestAPI,
    start_gds,
):
    """Keep the ground authentication sequence number aligned across in-suite
    reboots (issue #473 CI cascade).

    The TcSecurityDeframer persists a write-ahead high-water mark (issue #461),
    so after a reboot the board can legitimately expect a sequence number ahead
    of the ground counter, and every authenticated command is rejected until
    ground catches up. Reboots happen mid-suite (safe-mode entry, reset tests,
    watchdog tests), so before each test read the board's counter via
    GET_SEQ_NUM (bypass-listed, works even while desynced) and fast-forward the
    framer plugin's sequence file if the board is ahead. Ground being ahead is
    normal and left alone (the acceptance window extends forward).
    """
    # Don't recurse into the dedicated sync/format plumbing tests.
    if request.node.get_closest_marker("sync_sequence_number") or (
        request.node.get_closest_marker("format_filesystem")
    ):
        yield
        return

    link = request.config.getoption("--sync-deframer", default=None)
    if link is None:
        link = (
            "lora"
            if request.config.getoption("--with-radio", default=False)
            else "uart"
        )
    deframer = {
        "uart": "ComCcsdsUart.tcSecurityDeframer",
        "lora": "ComCcsdsLora.tcSecurityDeframer",
    }[link]

    try:
        resync_sequence_number(fprime_test_api, deframer)
    except Exception:  # noqa: BLE001 -- recovery must never fail a test itself
        pass
    yield


@pytest.fixture(autouse=True)
def recover_from_safe_mode(
    request: pytest.FixtureRequest,
    fprime_test_api: IntegrationTestAPI,
    start_gds,
):
    """Best-effort: after each test, if FSW slipped into SAFE_MODE (e.g. low
    voltage brownout from a burnwire-heavy test, or a partial file upload
    triggering SafeModeSequenceFailed), send EXIT_SAFE_MODE so the next test
    doesn't inherit the broken state. Only runs in the radio pass — UART
    tests that exercise mode transitions handle their own cleanup.
    """
    yield
    if not request.config.getoption("--with-radio"):
        return
    try:
        fprime_test_api.clear_histories()
        fprime_test_api.send_and_assert_command(
            "ReferenceDeployment.modeManager.GET_CURRENT_MODE",
            timeout=5,
            max_delay=5,
        )
        evt = fprime_test_api.await_event(
            "ReferenceDeployment.modeManager.CurrentModeReading", timeout=2
        )
        if evt is not None and "SAFE_MODE" in str(evt.args[0].val).upper():
            fprime_test_api.send_command(
                "ReferenceDeployment.modeManager.EXIT_SAFE_MODE"
            )
    except Exception:
        pass


@pytest.fixture(scope="session", autouse=True)
def stop_radio(
    request: pytest.FixtureRequest, fprime_test_api_session: IntegrationTestAPI
):
    """Fixture to stop the radio at the end of the test session"""
    with_radio = request.config.getoption("--with-radio")

    yield

    if not with_radio:
        return

    fprime_test_api_session.send_command(
        command="ReferenceDeployment.lora.TRANSMIT", args=["DISABLED"]
    )


@pytest.fixture(scope="session", autouse=True)
def tlm_sampler(
    request: pytest.FixtureRequest,
    fprime_test_api_session: IntegrationTestAPI,
):
    """Background sampler: log every telemetry update arriving on the radio link
    to a CSV. Lets us correlate link degradation against FSW-side counters
    (cmdDisp.CommandsDispatched/Dropped, lora.BytesSent/LastRssi, comQueue
    depth, etc). Only runs when --with-radio is set. File path overridable
    via TLM_SAMPLE_LOG env var (default: tlm_sample.csv in cwd).
    """
    if not request.config.getoption("--with-radio"):
        yield
        return

    log_path = os.environ.get("TLM_SAMPLE_LOG", "tlm_sample.csv")
    subhist = fprime_test_api_session.get_telemetry_subhistory()
    stop = threading.Event()

    def sample_loop():
        with open(log_path, "w", buffering=1) as fh:
            w = csv.writer(fh)
            w.writerow(["wall_ts", "channel", "value", "fsw_time"])
            while not stop.is_set():
                try:
                    for item in subhist.retrieve_new():
                        tmpl = item.get_template()
                        name = (
                            tmpl.get_full_name()
                            if hasattr(tmpl, "get_full_name")
                            else getattr(tmpl, "name", "?")
                        )
                        w.writerow(
                            [
                                f"{time.time():.3f}",
                                name,
                                item.get_val(),
                                str(item.get_time()),
                            ]
                        )
                except Exception as e:
                    w.writerow([f"{time.time():.3f}", "SAMPLER_ERROR", str(e), ""])
                stop.wait(1.0)

    t = threading.Thread(target=sample_loop, name="tlm_sampler", daemon=True)
    t.start()
    yield
    stop.set()
    t.join(timeout=3)
    fprime_test_api_session.remove_telemetry_subhistory(subhist)
