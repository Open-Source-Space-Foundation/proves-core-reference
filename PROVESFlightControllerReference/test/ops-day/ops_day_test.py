"""ops_day_test.py:

A compressed day of Electra-style operations on one board, from a fresh flash
image to the reboot that follows a comm loss. This is tier TS of the
day-in-the-life qualification (issue #625); the older manual bench tests in
test/day-in-the-life/ are separate. The steps run in file order and share the
board's state, as passes do: D10 checks the file D6 uplinks, and the comm loss
in D9 and the reboot in D10 run the flight sequences D8 puts on board. Each
step is named for the design report's gate it checks (D1 to D10). D7
(FileUplink holes) needs raw frames and D11 needs frame_judge, so neither runs
here yet.

Run it on a sim started on a fresh run directory:

    sim/run-sil.sh /tmp/ops-day && make sim-ops-day SIM_RUN=/tmp/ops-day
"""

import random
import time
import zlib
from itertools import pairwise
from pathlib import Path

import pytest
from common import (
    SEQUENCE_NUMBER_FILE,
    proves_send_and_assert_command,
    write_next_sequence_number,
)
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

mode_manager = "ReferenceDeployment.modeManager"
startup_manager = "ReferenceDeployment.startupManager"
deframer = "ComCcsdsUart.tcSecurityDeframer"
file_manager = "FileHandling.fileManager"
cmd_seq = "ReferenceDeployment.cmdSeq"
divider = "ReferenceDeployment.telemetryDelay.DIVIDER_PRM_SET"
# A Beacon-only member, so each update is one Beacon.
beacon_channel = f"{deframer}.CurrentSequenceNumber"

DAY_FILE = "/ops/day.bin"
DAY_DATA = random.Random(1026).randbytes(3000)
# CalculateCrc reports the CRC-32 without its final inversion, which the ops tools expect.
DAY_CRC = zlib.crc32(DAY_DATA) ^ 0xFFFFFFFF
# Os::File::DOESNT_EXIST, as CalculateCrcFailed reports it.
OS_FILE_DOESNT_EXIST = 11
SEQUENCE = Path(__file__).parent / "ops_day.seq"
# The flight sequences that the comm loss (enter_safe) and the D10 reboot (startup) run.
FLIGHT_SEQUENCES = [
    Path(__file__).parents[3] / "sequences" / name
    for name in ("enter_safe.seq", "startup.seq")
]


def boot_count(api: IntegrationTestAPI) -> int:
    proves_send_and_assert_command(api, f"{startup_manager}.GET_BOOT_COUNT")
    return (
        api.assert_event(f"{startup_manager}.CurrentBootCount", timeout=3).args[0].val
    )


def current_mode(api: IntegrationTestAPI) -> str:
    proves_send_and_assert_command(api, f"{mode_manager}.GET_CURRENT_MODE")
    return str(
        api.assert_event(f"{mode_manager}.CurrentModeReading", timeout=3).args[0].val
    )


def board_sequence_number(api: IntegrationTestAPI) -> int:
    api.clear_histories()
    api.send_command(f"{deframer}.GET_SEQ_NUM")
    return api.assert_event(f"{deframer}.SequenceNumberGet", timeout=5).args[0].val


def last_sent_sequence_number() -> int:
    return int(Path(SEQUENCE_NUMBER_FILE).read_text()) - 1


def crc(api: IntegrationTestAPI, path: str) -> int:
    proves_send_and_assert_command(api, f"{file_manager}.CalculateCrc", [path])
    return (
        api.assert_event(f"{file_manager}.CalculateCrcSucceeded", timeout=5).args[1].val
    )


def beacon_times(api: IntegrationTestAPI, count: int, timeout: float) -> list[float]:
    """FSW times, in seconds, of the next `count` Beacons."""
    history = api.get_telemetry_subhistory(
        api.get_telemetry_pred(beacon_channel), fsw_order=False
    )
    try:
        assert api.await_telemetry_count(count, history=history, timeout=timeout), (
            f"fewer than {count} Beacons in {timeout} s"
        )
        return [item.get_time().get_float() for item in history.retrieve()[:count]]
    finally:
        api.remove_telemetry_subhistory(history)


# A fresh image has no parameter file, sequence-number files or /seq/startup.bin
# yet. The step sees these boot events only when it connects before they are sent.
@pytest.mark.expect_warnings(
    "FileHandling.prmDb.PrmFileReadError",
    f"{deframer}.SequenceNumberReadFailed",
    "ComCcsdsLora.tcSecurityDeframer.SequenceNumberReadFailed",
    f"{cmd_seq}.CS_FileReadError",
)
def test_d1_clean_boot(fprime_test_api: IntegrationTestAPI):
    """A fresh image boots once, in NORMAL mode, and takes commands"""
    deadline = time.time() + 30
    while True:
        try:
            fprime_test_api.send_and_assert_command(
                "CdhCore.cmdDisp.CMD_NO_OP", timeout=2
            )
            break
        except AssertionError:
            if time.time() > deadline:
                raise
    assert boot_count(fprime_test_api) == 1
    assert current_mode(fprime_test_api) == "NORMAL"


def test_d2_beacon_carries_the_sequence_number(fprime_test_api: IntegrationTestAPI):
    """The Beacon reports the last sequence number the board accepted"""
    expected = board_sequence_number(fprime_test_api)
    fprime_test_api.clear_histories()
    fprime_test_api.assert_telemetry(beacon_channel, expected, timeout=35)


@pytest.mark.expect_warnings(f"{deframer}.SequenceNumberInvalid")
def test_d3_replay_rejected(fprime_test_api: IntegrationTestAPI):
    """A frame that repeats an accepted sequence number is rejected"""
    accepted = board_sequence_number(fprime_test_api)
    write_next_sequence_number(accepted - 2)
    fprime_test_api.clear_histories()
    fprime_test_api.send_command("CdhCore.cmdDisp.CMD_NO_OP_STRING", ["replayed"])
    fprime_test_api.assert_event(f"{deframer}.SequenceNumberInvalid", timeout=5)
    fprime_test_api.assert_event_count(
        0, "CdhCore.cmdDisp.NoOpStringReceived", timeout=2
    )
    write_next_sequence_number(accepted)


def test_d3_forward_jump_accepted(fprime_test_api: IntegrationTestAPI):
    """A frame ahead of the board within the window is accepted"""
    accepted = board_sequence_number(fprime_test_api)
    write_next_sequence_number(accepted + 1000)
    proves_send_and_assert_command(
        fprime_test_api, "CdhCore.cmdDisp.CMD_NO_OP_STRING", ["ahead"], retries=1
    )
    assert (
        board_sequence_number(fprime_test_api)
        == last_sent_sequence_number()
        > accepted + 1000
    )


def test_d5a_divider_7_gives_an_8_s_beacon(fprime_test_api: IntegrationTestAPI):
    """Divider 7, the pass setting, gives an 8 s Beacon"""
    proves_send_and_assert_command(fprime_test_api, divider, ["7"])
    times = beacon_times(fprime_test_api, 3, timeout=30)
    assert [round(b - a) for a, b in pairwise(times)] == [8, 8]


# Runs under divider 7, as on a pass, so the next telemetry tick is at most 8 s away.
def test_d4_send_pkt_sends_the_radio_packet(fprime_test_api: IntegrationTestAPI):
    """SEND_PKT 8, the ops link probe, reports PacketSent and the Radio packet follows on the next tick"""
    proves_send_and_assert_command(
        fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["8", "REALTIME"]
    )
    fprime_test_api.assert_event("CdhCore.tlmSend.PacketSent", [8], timeout=3)
    fprime_test_api.assert_telemetry("ReferenceDeployment.lora.LastRssi", timeout=10)


def test_d5b_divider_29_restores_the_30_s_beacon(fprime_test_api: IntegrationTestAPI):
    """Divider 29, the default, restores the 30 s Beacon"""
    proves_send_and_assert_command(fprime_test_api, divider, ["29"])
    times = beacon_times(fprime_test_api, 2, timeout=70)
    assert round(times[1] - times[0]) == 30


def test_d6_file_operations_and_crc(
    fprime_test_api: IntegrationTestAPI, tmp_path: Path
):
    """An uplinked file lists, sizes and CRCs as on the ground"""
    local = tmp_path / "day.bin"
    local.write_bytes(DAY_DATA)
    proves_send_and_assert_command(
        fprime_test_api, f"{file_manager}.CreateDirectory", ["/ops"]
    )
    assert fprime_test_api.uplink_file_and_await_completion(
        str(local), DAY_FILE, timeout=30
    )

    proves_send_and_assert_command(
        fprime_test_api, f"{file_manager}.FileSize", [DAY_FILE]
    )
    fprime_test_api.assert_event(
        f"{file_manager}.FileSizeSucceeded", [DAY_FILE, len(DAY_DATA)], timeout=3
    )
    proves_send_and_assert_command(
        fprime_test_api, f"{file_manager}.ListDirectory", ["/ops"]
    )
    fprime_test_api.assert_event(
        f"{file_manager}.DirectoryListing",
        ["/ops", "day.bin", len(DAY_DATA)],
        timeout=3,
    )
    assert crc(fprime_test_api, DAY_FILE) == DAY_CRC


@pytest.mark.expect_warnings(f"{file_manager}.CalculateCrcFailed")
def test_d6_crc_of_a_missing_file_fails(fprime_test_api: IntegrationTestAPI):
    """CalculateCrc on a missing file fails with the status the ops tools parse"""
    fprime_test_api.clear_histories()
    fprime_test_api.send_command(f"{file_manager}.CalculateCrc", ["/ops/missing.bin"])
    fprime_test_api.assert_event(
        f"{file_manager}.CalculateCrcFailed",
        ["/ops/missing.bin", OS_FILE_DOESNT_EXIST],
        timeout=3,
    )


def test_d8_flight_sequences_compile_and_validate(fprime_test_api: IntegrationTestAPI):
    """The flight enter_safe and startup sequences compile, uplink and validate; D9 and D10 run them"""
    proves_send_and_assert_command(
        fprime_test_api, f"{file_manager}.CreateDirectory", ["/seq"]
    )
    for sequence in FLIGHT_SEQUENCES:
        on_board = f"/seq/{sequence.stem}.bin"
        assert fprime_test_api.uplink_sequence_and_await_completion(
            str(sequence), on_board, timeout=30
        )
        proves_send_and_assert_command(
            fprime_test_api, f"{cmd_seq}.CS_VALIDATE", [on_board]
        )
        fprime_test_api.assert_event(f"{cmd_seq}.CS_SequenceValid", timeout=3)


@pytest.mark.expect_warnings(f"{cmd_seq}.CS_InvalidMode")
def test_d8_sequence_validates_runs_and_refuses_a_second_run(
    fprime_test_api: IntegrationTestAPI,
):
    """An uplinked sequence validates, runs with its delays and refuses a second run"""
    assert fprime_test_api.uplink_sequence_and_await_completion(
        str(SEQUENCE), "/seq/ops_day.bin", timeout=30
    )
    proves_send_and_assert_command(
        fprime_test_api, f"{cmd_seq}.CS_VALIDATE", ["/seq/ops_day.bin"]
    )
    fprime_test_api.assert_event(f"{cmd_seq}.CS_SequenceValid", timeout=3)

    fprime_test_api.clear_histories()
    fprime_test_api.send_command(f"{cmd_seq}.CS_RUN", ["/seq/ops_day.bin", "BLOCK"])
    fprime_test_api.assert_event(f"{cmd_seq}.CS_SequenceLoaded", timeout=3)
    fprime_test_api.send_command(f"{cmd_seq}.CS_RUN", ["/seq/ops_day.bin", "NO_BLOCK"])
    fprime_test_api.assert_event(f"{cmd_seq}.CS_InvalidMode", timeout=3)
    fprime_test_api.assert_event(f"{cmd_seq}.CS_SequenceComplete", timeout=10)

    steps = fprime_test_api.get_event_test_history().retrieve()
    completed = [
        e.get_time().get_float()
        for e in steps
        if e.get_template().get_full_name() == f"{cmd_seq}.CS_CommandComplete"
    ]
    assert len(completed) == 3
    assert round(completed[1] - completed[0]) == 2
    assert round(completed[2] - completed[1]) == 1


@pytest.mark.expect_warnings(
    f"{mode_manager}.CommandLossDetected",
    f"{mode_manager}.EnteringSafeMode",
)
def test_d9_comm_loss_enters_safe_mode_and_stays_commandable(
    fprime_test_api: IntegrationTestAPI,
):
    """With no uplink for COMM_LOSS_TIME the board enters safe mode, keeps beaconing and takes commands"""
    # The 8 s Beacon shows safe mode well before the watchdog reboot 44 s after entry.
    proves_send_and_assert_command(fprime_test_api, divider, ["7"])
    proves_send_and_assert_command(
        fprime_test_api,
        f"{mode_manager}.COMM_LOSS_TIME_PRM_SET",
        ['{"seconds": 10, "useconds": 0}'],
    )
    fprime_test_api.assert_event(f"{mode_manager}.CommandLossDetected", timeout=20)
    fprime_test_api.assert_event(f"{mode_manager}.EnteringSafeMode", timeout=10)
    fprime_test_api.assert_event(
        f"{mode_manager}.SafeModeSequenceCompleted", timeout=15
    )
    fprime_test_api.assert_telemetry(f"{mode_manager}.CurrentMode", 1, timeout=10)
    assert current_mode(fprime_test_api) == "SAFE_MODE"


# The 10 s COMM_LOSS_TIME from D9 fires again while this step waits without
# uplinking, until the reboot restores the default.
@pytest.mark.expect_warnings(
    "FileHandling.prmDb.PrmFileReadError",
    f"{mode_manager}.CommandLossDetected",
    f"{mode_manager}.EnteringSafeMode",
)
def test_d10_power_cycle_keeps_state(fprime_test_api: IntegrationTestAPI):
    """The watchdog reboot after comm loss runs the start-up sequence and keeps the boot count, safe mode, files and sequence number"""
    last_accepted = last_sent_sequence_number()
    rebooted = fprime_test_api.get_telemetry_subhistory(
        fprime_test_api.get_telemetry_pred(f"{startup_manager}.BootCount", 2),
        fsw_order=False,
    )
    beacons = fprime_test_api.get_telemetry_subhistory(
        fprime_test_api.get_telemetry_pred(beacon_channel), fsw_order=False
    )
    startup_loaded = fprime_test_api.get_event_subhistory(
        fprime_test_api.get_event_pred(
            f"{cmd_seq}.CS_SequenceLoaded", ["/seq/startup.bin"]
        ),
        fsw_order=False,
    )
    try:
        # Wait without uplinking, so the first Beacon after the reboot shows the
        # sequence number the board restored, not one a new frame set.
        assert fprime_test_api.await_telemetry_count(
            1, history=rebooted, timeout=150
        ), "no Beacon with BootCount 2 within 150 s of comm loss"
        # The Beacon decodes BootCount before the sequence number.
        time.sleep(1)
        assert beacons.retrieve()[-1].get_val() == last_accepted
        assert startup_loaded.size() == 1
    finally:
        fprime_test_api.remove_telemetry_subhistory(rebooted)
        fprime_test_api.remove_telemetry_subhistory(beacons)
        fprime_test_api.remove_event_subhistory(startup_loaded)

    assert current_mode(fprime_test_api) == "SAFE_MODE"
    proves_send_and_assert_command(
        fprime_test_api, f"{mode_manager}.GET_SAFE_MODE_REASON"
    )
    reason = fprime_test_api.assert_event(
        f"{mode_manager}.CurrentSafeModeReasonReading", timeout=3
    )
    assert str(reason.args[0].val) == "COMMAND_LOSS"
    assert crc(fprime_test_api, DAY_FILE) == DAY_CRC

    proves_send_and_assert_command(fprime_test_api, f"{mode_manager}.EXIT_SAFE_MODE")
    assert current_mode(fprime_test_api) == "NORMAL"
