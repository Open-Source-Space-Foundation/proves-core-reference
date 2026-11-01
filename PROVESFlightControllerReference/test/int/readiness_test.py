"""
readiness_test.py:

Bench readiness gate. Runs before the integration suite and fails when the
board is not fit to test, so a broken bench is not blamed on the PR under test.

The checks only read what the flight software already reports: the Health
packets, the root directory listing, free space, boot count, battery voltage,
and the onboard ReadinessMonitor masks once that component is in the build.
watchdog_test.py::test_03 carries the same marker and proves the watchdog can
reset the board, which nothing onboard can see.

Run with `make test-readiness`. To also check a flight unit, pass
--unit-manifest=<file.json> holding
    {"project_version": "<CdhCore.version.ProjectVersion>",
     "file_sources": {"/seq/startup.bin": "<local startup.bin>"}}
and the suite compares the version and each board file's CRC.
"""

import json
import re
import time
import zlib

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

pytestmark = [pytest.mark.readiness]

FILE_MANAGER = "FileHandling.fileManager"
STARTUP_MANAGER = "ReferenceDeployment.startupManager"
READINESS_MONITOR = "ReferenceDeployment.readinessMonitor"
VERSION = "CdhCore.version"

BEACON_PACKET_ID = "1"
HEALTH_PACKET_ID = "2"
HEALTH_WARNINGS_PACKET_ID = "3"
FILE_SYSTEM_PACKET_ID = "5"

# Counters that must not move while the board sits idle. A nonzero absolute
# value is history (boot, earlier runs); growth during the quiet window means
# the board is unhealthy now.
IDLE_COUNTER_BUDGETS = {
    "CdhCore.cmdDisp.CommandsDropped": 0,
    "CdhCore.health.PingLateWarnings": 0,
    "ComCcsdsLora.commsBufferManager.NoBuffs": 0,
    "ComCcsdsUart.commsBufferManager.NoBuffs": 0,
    "ReferenceDeployment.payloadBufferManager.NoBuffs": 0,
    "ReferenceDeployment.rateGroup50Hz.RgCycleSlips": 0,
    "ReferenceDeployment.rateGroup10Hz.RgCycleSlips": 0,
    "ReferenceDeployment.rateGroup1Hz.RgCycleSlips": 0,
    "ReferenceDeployment.comDriver.RxOverrunCount": 0,
    "ReferenceDeployment.comDriver.TxDropCount": 0,
}
QUIET_WINDOW_S = 30

# Matches the ReadinessMonitor FS_SPACE default floor.
FS_FREE_FLOOR_BYTES = 256 * 1024
REQUIRED_ROOT_FILES = {"boot_count.bin"}
# The no-lock FatFs root wipe left a 0-byte entry with a junk name and files
# whose long names fell back to 8.3 aliases such as FSC~A8D6.BIN.
LOST_LONG_NAME = re.compile(r"~[0-9A-F]", re.IGNORECASE)

# ModeManager.SafeModeEntryVoltage default. Below it the board drops into
# safe mode mid-run and powers the faces off.
SAFE_MODE_ENTRY_VOLTAGE = 6.7

# Bit n of the onboard masks is ReadinessCheck enum value n. Append-only.
READINESS_CHECKS = [
    "FS_ROOT",
    "FS_SPACE",
    "DOWNLINK",
    "FACE_SENSORS",
    "MTQ_DRIVERS",
    "I2C_BUS",
    "WDT_PET",
    "RESET_CAUSE",
    "BATTERY",
    "FLIGHT_CONFIG",
    "CLOCK_BOOT",
    "RADIO",
]


def request_packets(
    api: IntegrationTestAPI, packet_ids: list[str], channels: set[str]
) -> dict[str, float]:
    """Force-send packets and return the latest value of each wanted channel.

    A forced packet can be lost to a corrupted frame, so resend until every
    wanted channel has arrived or the attempts run out.
    """
    latest: dict[str, float] = {}
    for _ in range(4):
        api.clear_histories()
        for packet_id in packet_ids:
            api.send_command("CdhCore.tlmSend.SEND_PKT", [packet_id, "REALTIME"])
        time.sleep(5)
        for update in api.get_telemetry_test_history().retrieve():
            name = update.template.get_full_name()
            if name in channels:
                latest[name] = update.get_val()
        if channels <= latest.keys():
            break
    return latest


def list_root(api: IntegrationTestAPI) -> dict[str, int]:
    """Return {name: size} for the files in "/".

    FileManager rejects a listing that overlaps another one, so retry.
    """
    for _ in range(3):
        api.clear_histories()
        api.send_command(f"{FILE_MANAGER}.ListDirectory", ["/"])
        if api.await_event(f"{FILE_MANAGER}.ListDirectorySucceeded", timeout=15):
            return {
                str(e.args[1].val).rsplit("/", 1)[-1]: int(e.args[2].val)
                for e in api.get_event_test_history().retrieve()
                if e.template.get_full_name() == f"{FILE_MANAGER}.DirectoryListing"
            }
    raise AssertionError('ListDirectory "/" did not succeed in 3 attempts')


def board_crc(api: IntegrationTestAPI, path: str) -> int:
    """Return fileManager.CalculateCrc for a file on the board."""
    for _ in range(3):
        api.clear_histories()
        api.send_command(f"{FILE_MANAGER}.CalculateCrc", [path])
        evt = api.await_event(f"{FILE_MANAGER}.CalculateCrcSucceeded", timeout=15)
        if evt is not None:
            return int(evt.args[1].val)
    raise AssertionError(f"CalculateCrc {path} did not succeed in 3 attempts")


def mask_table(pass_mask: int, warn_mask: int, fail_mask: int) -> str:
    """Render the onboard masks as one line per check."""
    rows = []
    for bit, name in enumerate(READINESS_CHECKS):
        if fail_mask >> bit & 1:
            state = "FAIL"
        elif warn_mask >> bit & 1:
            state = "WARN"
        elif pass_mask >> bit & 1:
            state = "PASS"
        else:
            state = "NOT_RUN"
        rows.append(f"  {bit:2d} {name:<14} {state}")
    return "\n".join(rows)


@pytest.fixture(scope="module")
def unit_manifest(request: pytest.FixtureRequest) -> dict | None:
    """Load --unit-manifest, or None when no manifest was given."""
    path = request.config.getoption("--unit-manifest", default=None)
    if path is None:
        return None
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def test_01_idle_health_counters_hold_still(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """No drops, slips, late pings, or buffer exhaustion while the board idles."""
    channels = set(IDLE_COUNTER_BUDGETS)
    unknown = channels - fprime_test_api.dictionaries.channel_name.keys()
    assert not unknown, f"channels not in this build's dictionary: {sorted(unknown)}"
    packets = [HEALTH_PACKET_ID, HEALTH_WARNINGS_PACKET_ID]

    before = request_packets(fprime_test_api, packets, channels)
    missing = channels - before.keys()
    assert not missing, f"Health packets never reported: {sorted(missing)}"

    time.sleep(QUIET_WINDOW_S)
    after = request_packets(fprime_test_api, packets, channels)
    missing = channels - after.keys()
    assert not missing, f"Health packets stopped reporting: {sorted(missing)}"

    print(f"Health counters over a {QUIET_WINDOW_S} s idle window:")
    over_budget = []
    for name, budget in IDLE_COUNTER_BUDGETS.items():
        growth = after[name] - before[name]
        print(f"  {name}: {before[name]} -> {after[name]}")
        if growth > budget:
            over_budget.append(f"{name} grew by {growth} (budget {budget})")
    assert not over_budget, "board unhealthy while idle: " + "; ".join(over_budget)


def test_02_filesystem_root_intact(fprime_test_api: IntegrationTestAPI, start_gds):
    """Root lists, holds the files FSW needs, and has no garbage entries."""
    entries = list_root(fprime_test_api)
    print("Root listing:\n" + "\n".join(f"  {n} ({s} B)" for n, s in entries.items()))

    garbage = [
        f"{n!r} ({size} B)"
        for n, size in entries.items()
        if not n.isprintable() or LOST_LONG_NAME.search(n) or size == 0
    ]
    assert not garbage, f"root directory has corrupt-looking entries: {garbage}"

    missing = REQUIRED_ROOT_FILES - entries.keys()
    assert not missing, f"root directory is missing {sorted(missing)}"


def test_03_filesystem_has_free_space(fprime_test_api: IntegrationTestAPI, start_gds):
    """Free space is above the floor FSW needs for logs and uplinks."""
    free_channel = "ReferenceDeployment.fsSpace.FreeSpace"
    total_channel = "ReferenceDeployment.fsSpace.TotalSpace"
    values = request_packets(
        fprime_test_api, [FILE_SYSTEM_PACKET_ID], {free_channel, total_channel}
    )
    assert free_channel in values, "FileSystem packet never reported FreeSpace"
    print(f"Free space: {values[free_channel]} of {values.get(total_channel)} B")
    assert values[free_channel] >= FS_FREE_FLOOR_BYTES, (
        f"only {values[free_channel]} B free, floor is {FS_FREE_FLOOR_BYTES} B"
    )


def test_04_boot_count_readable(fprime_test_api: IntegrationTestAPI, start_gds):
    """StartupManager answers with a plausible boot count."""
    proves_send_and_assert_command(fprime_test_api, f"{STARTUP_MANAGER}.GET_BOOT_COUNT")
    evt = fprime_test_api.assert_event(f"{STARTUP_MANAGER}.CurrentBootCount", timeout=3)
    boot_count = evt.args[0].val
    print(f"Boot count: {boot_count}")
    assert boot_count > 0, f"boot count {boot_count} is not plausible"


@pytest.mark.requires_battery
def test_05_battery_above_safe_mode_entry(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """System voltage is high enough that the run will not trip safe mode."""
    manager = "ReferenceDeployment.ina219SysManager"
    proves_send_and_assert_command(fprime_test_api, f"{manager}.GetVoltage")
    evt = fprime_test_api.assert_event(f"{manager}.VoltageReading", timeout=3)
    voltage = evt.args[0].val
    print(f"System voltage: {voltage:.2f} V")
    assert voltage > SAFE_MODE_ENTRY_VOLTAGE, (
        f"system voltage {voltage:.2f} V is at or below the "
        f"{SAFE_MODE_ENTRY_VOLTAGE} V safe-mode entry threshold"
    )


def test_06_onboard_readiness_masks_clean(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """The onboard ReadinessMonitor runs a full pass and reports no failures."""
    if (
        f"{READINESS_MONITOR}.RUN_CHECKS"
        not in fprime_test_api.dictionaries.command_name
    ):
        pytest.xfail("ReadinessMonitor is not in this build's dictionary")

    proves_send_and_assert_command(fprime_test_api, f"{READINESS_MONITOR}.RUN_CHECKS")
    summary = fprime_test_api.await_event(
        f"{READINESS_MONITOR}.ReadinessSummary", timeout=30
    )
    assert summary is not None, "no ReadinessSummary within 30 s of RUN_CHECKS"

    masks = {
        f"{READINESS_MONITOR}.{name}": name
        for name in ("PassMask", "WarnMask", "FailMask")
    }
    values = request_packets(fprime_test_api, [BEACON_PACKET_ID], set(masks))
    missing = set(masks) - values.keys()
    assert not missing, f"Beacon never reported {sorted(missing)}"
    by_name = {masks[k]: int(v) for k, v in values.items()}

    print(
        "Onboard readiness:\n"
        + mask_table(by_name["PassMask"], by_name["WarnMask"], by_name["FailMask"])
    )
    assert by_name["FailMask"] == 0, (
        f"onboard readiness FailMask is {by_name['FailMask']:#x}"
    )


def test_07_unit_matches_manifest(
    fprime_test_api: IntegrationTestAPI, start_gds, unit_manifest
):
    """A flight unit runs the expected firmware and carries the expected files."""
    if unit_manifest is None:
        pytest.skip("no --unit-manifest given")

    expected_version = unit_manifest.get("project_version")
    if expected_version is not None:
        proves_send_and_assert_command(
            fprime_test_api, f"{VERSION}.VERSION", ["PROJECT"]
        )
        evt = fprime_test_api.assert_event(f"{VERSION}.ProjectVersion", timeout=10)
        actual = str(evt.args[0].val)
        assert actual == expected_version, (
            f"firmware {actual!r}, manifest expects {expected_version!r}"
        )

    mismatched = []
    for path, local_file in unit_manifest.get("file_sources", {}).items():
        with open(local_file, "rb") as f:
            expected = zlib.crc32(f.read()) ^ 0xFFFFFFFF
        actual = board_crc(fprime_test_api, path)
        if actual != expected:
            mismatched.append(f"{path} CRC {actual:#010x}, expected {expected:#010x}")
    assert not mismatched, "; ".join(mismatched)
