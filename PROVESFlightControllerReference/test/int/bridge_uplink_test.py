"""
End-to-end bridge test: GDS -> GRC data port (21203) -> LoRa -> flight board.

Validates the authenticated command/file-uplink link through the GRC LoRa
bridge. Run with fprime-gds already attached to the GRC data port.
"""

import hashlib
import os

import pytest
from fprime_gds.common.testing_fw.api import IntegrationTestAPI


def test_noop(fprime_test_api: IntegrationTestAPI):
    """Tracer bullet: a single authenticated NO_OP must round-trip over LoRa."""
    fprime_test_api.send_and_assert_command(
        "CdhCore.cmdDisp.CMD_NO_OP", timeout=30, max_delay=30
    )


def test_get_seq(fprime_test_api: IntegrationTestAPI):
    """Send bypass GET_SEQ_NUM; the seq is emitted to the flight console."""
    fprime_test_api.send_command("ComCcsdsLora.authenticatelora.GET_SEQ_NUM")
    print(
        "[seq] GET_SEQ_NUM sent over LoRa (watch flight console for EmitSequenceNumber)"
    )
    import time as _t

    _t.sleep(8)


def test_spray_seq(fprime_test_api: IntegrationTestAPI):
    """Spray many bypass GET_SEQ_NUM rapidly to land one in a flight RX window."""
    import time as _t

    n = int(os.environ.get("SPRAY_N", "20"))
    gap = float(os.environ.get("SPRAY_GAP", "0.3"))
    for i in range(n):
        fprime_test_api.send_command("ComCcsdsLora.authenticatelora.GET_SEQ_NUM")
        _t.sleep(gap)
    print(f"[spray] sent {n} GET_SEQ_NUM @ {gap}s")
    _t.sleep(5)


def test_spray_disable_tx(fprime_test_api: IntegrationTestAPI):
    """Spray lora.TRANSMIT TX_STATE (default DISABLED) rapidly (non-bypass; needs synced seq)."""
    import time as _t

    state = os.environ.get("TX_STATE", "DISABLED")
    n = int(os.environ.get("SPRAY_N", "20"))
    gap = float(os.environ.get("SPRAY_GAP", "0.3"))
    for i in range(n):
        fprime_test_api.send_command("ReferenceDeployment.lora.TRANSMIT", [state])
        _t.sleep(gap)
    print(f"[spray] sent {n} lora.TRANSMIT {state} @ {gap}s")
    _t.sleep(5)


def test_enable_tx(fprime_test_api: IntegrationTestAPI):
    """Enable flight LoRa transmit + set a fast downlink divider (non-bypass: needs synced seq)."""
    div = int(os.environ.get("DOWNLINK_DIVIDER", "5"))
    fprime_test_api.send_command(
        "ReferenceDeployment.downlinkDelay.DIVIDER_PRM_SET", [div]
    )
    fprime_test_api.send_command("ReferenceDeployment.lora.TRANSMIT", ["ENABLED"])
    print(f"[tx] sent DIVIDER_PRM_SET({div}) + lora.TRANSMIT(ENABLED) over LoRa")
    import time as _t

    _t.sleep(8)


def test_append_file(fprime_test_api: IntegrationTestAPI):
    """Spray fileManager.AppendFile APPEND_SRC -> APPEND_TGT (non-bypass; reassembles parts)."""
    import time as _t

    src = os.environ["APPEND_SRC"]
    tgt = os.environ["APPEND_TGT"]
    n = int(os.environ.get("SPRAY_N", "12"))
    gap = float(os.environ.get("SPRAY_GAP", "0.3"))
    for i in range(n):
        fprime_test_api.send_command("FileHandling.fileManager.AppendFile", [src, tgt])
        _t.sleep(gap)
    print(f"[append] sprayed {n} AppendFile {src} -> {tgt}")
    _t.sleep(5)


def test_remove_file(fprime_test_api: IntegrationTestAPI):
    """Spray fileManager.RemoveFile RM_FILE (ignoreErrors=True) to clear stale dests."""
    import time as _t

    f = os.environ["RM_FILE"]
    n = int(os.environ.get("SPRAY_N", "10"))
    gap = float(os.environ.get("SPRAY_GAP", "0.3"))
    for i in range(n):
        fprime_test_api.send_command("FileHandling.fileManager.RemoveFile", [f, True])
        _t.sleep(gap)
    print(f"[rm] sprayed {n} RemoveFile {f}")
    _t.sleep(3)


def test_calc_crc(fprime_test_api: IntegrationTestAPI):
    """Spray fileManager.CalculateCrc over CRC_FILE (non-bypass; needs synced seq).

    Emits CalculateCrcSucceeded : <file> has CRC value 0x.. on the flight console.
    Read-back oracle for verifying a patched file against the local truth CRC.
    """
    import time as _t

    f = os.environ.get("CRC_FILE", "//ota_q1.bin")
    n = int(os.environ.get("SPRAY_N", "15"))
    gap = float(os.environ.get("SPRAY_GAP", "0.3"))
    for i in range(n):
        fprime_test_api.send_command("FileHandling.fileManager.CalculateCrc", [f])
        _t.sleep(gap)
    print(
        f"[crc] sprayed {n} CalculateCrc {f} @ {gap}s (watch console for CalculateCrcSucceeded)"
    )
    _t.sleep(6)


def test_detect_drops(fprime_test_api: IntegrationTestAPI):
    """Spray dropDetector.DETECT_DROPS over the file just uplinked (non-bypass; needs synced seq).

    The board scans <DETECT_FILE> for runs of <DETECT_PKT> zero-bytes (dropped
    chunks left as zeros by FileUplink's offset writes) and emits PossibleDrop
    events (1-based chunk index) plus DetectingDrops/DetectingDropsCompleted on
    the flight console. Spray so one send lands in a flight RX window.
    """
    import time as _t

    f = os.environ.get("DETECT_FILE", "//ota_patch.bin")
    pkt = int(os.environ.get("DETECT_PKT", "204"))
    n = int(os.environ.get("SPRAY_N", "15"))
    gap = float(os.environ.get("SPRAY_GAP", "0.3"))
    for i in range(n):
        fprime_test_api.send_command(
            "ReferenceDeployment.dropDetector.DETECT_DROPS", [f, pkt]
        )
        _t.sleep(gap)
    print(
        f"[detect] sprayed {n} DETECT_DROPS {f} {pkt} @ {gap}s (watch console for PossibleDrop)"
    )
    _t.sleep(5)


@pytest.mark.parametrize(
    "src", [os.environ.get("UPLINK_SRC", "/tmp/grc_scratch_2k.bin")]
)
def test_uplink_file(fprime_test_api: IntegrationTestAPI, src):
    """Uplink a file over the LoRa bridge and await FileUplink completion."""
    dest = os.environ.get("UPLINK_DEST", "//bridge_test.bin")
    timeout = int(os.environ.get("UPLINK_TIMEOUT", "120"))
    with open(src, "rb") as f:
        digest = hashlib.md5(f.read()).hexdigest()
    print(
        f"\n[uplink] src={src} dest={dest} size={os.path.getsize(src)} md5={digest} timeout={timeout}s"
    )
    fprime_test_api.uplink_file_and_await_completion(src, dest, timeout=timeout)
    print(f"[uplink] completion received for {dest}")
