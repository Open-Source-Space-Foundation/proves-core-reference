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


def test_send_cmd(fprime_test_api: IntegrationTestAPI):
    """Fire-and-forget any command (no ack needed). env CMD, optional ARGS (comma-sep).

    Used to command the GRC over its (downlink-less) control port, e.g.
    CMD=ReferenceDeployment.uhf.DATA_RATE_PRM_SET ARGS=SF_7
    """
    import os as _os
    import time as _t

    cmd = _os.environ["CMD"]
    raw = _os.environ.get("ARGS", "")
    args = [a for a in raw.split(",") if a != ""]
    reps = int(_os.environ.get("REPS", "1"))
    gap = float(_os.environ.get("GAP", "0.4"))
    for _ in range(reps):
        fprime_test_api.send_command(cmd, args)
        _t.sleep(gap)
    print(f"[cmd] sent {cmd} {args} x{reps}")
    _t.sleep(2)


def test_watch_events(fprime_test_api: IntegrationTestAPI):
    """Passively watch this GDS's decoded event stream for WATCH_SECS seconds.

    Use against the FC direct GDS (21101) to read flight events even when the
    ASCII console capture is stale. env WATCH_SECS (default 20), WATCH_FILTER
    (substring; only matching events printed).
    """
    import os as _os
    import time as _t

    secs = float(_os.environ.get("WATCH_SECS", "20"))
    name = _os.environ.get(
        "WATCH_EVENT", "ComCcsdsLora.authenticatelora.EmitSequenceNumber"
    )
    pred = fprime_test_api.get_event_pred(event=name)
    sub = fprime_test_api.get_event_subhistory(event_filter=pred)
    print(f"[watch] collecting '{name}' for {secs}s ...")
    _t.sleep(secs)
    items = list(sub.retrieve())
    print(f"[watch] COUNT={len(items)} of '{name}' in {secs}s")
    for ev in items[-5:]:
        print(f"[ev] {ev}")


def test_watch_events_multi(fprime_test_api: IntegrationTestAPI):
    """Watch several event names at once for WATCH_SECS. env WATCH_EVENTS
    (comma-separated fully-qualified names), WATCH_SECS. Reports per-event COUNT
    and prints every captured instance. Used to passively capture the GRC
    ByteComBridge warning events during a transfer (point at GRC GDS 50060)."""
    import os as _os
    import time as _t

    secs = float(_os.environ.get("WATCH_SECS", "60"))
    names = [n for n in _os.environ.get("WATCH_EVENTS", "").split(",") if n]
    subs = {
        n: fprime_test_api.get_event_subhistory(
            event_filter=fprime_test_api.get_event_pred(event=n)
        )
        for n in names
    }
    print(f"[multi] watching {len(names)} events for {secs}s ...", flush=True)
    _t.sleep(secs)
    for n, sub in subs.items():
        items = list(sub.retrieve())
        print(f"[multi] COUNT={len(items)} {n}")
        for ev in items:
            print(f"[mev] {ev}")


def test_await_filerx(fprime_test_api: IntegrationTestAPI):
    """Watch the FC GDS (21101) for FileUplink completion, print wall-clock on first hit.

    Polls subhistories for FileReceived (success) and BadChecksum (over-air loss)
    up to WATCH_SECS. Prints `FRX <unix_time> <text>` / `BADCRC <unix_time> <text>`
    the instant one is seen, then exits — used to measure transfer time vs a t0
    recorded by the caller. env WATCH_SECS (default 120).
    """
    import time as _t

    secs = float(os.environ.get("WATCH_SECS", "120"))
    ok = fprime_test_api.get_event_subhistory(
        event_filter=fprime_test_api.get_event_pred(
            event="FileHandling.fileUplink.FileReceived"
        )
    )
    bad = fprime_test_api.get_event_subhistory(
        event_filter=fprime_test_api.get_event_pred(
            event="FileHandling.fileUplink.BadChecksum"
        )
    )
    print(
        f"[await] watching FileReceived/BadChecksum for up to {secs}s ...", flush=True
    )
    t_end = _t.time() + secs
    while _t.time() < t_end:
        oi = list(ok.retrieve())
        if oi:
            print(f"FRX {_t.time():.3f} {oi[-1]}", flush=True)
            return
        bi = list(bad.retrieve())
        if bi:
            print(f"BADCRC {_t.time():.3f} {bi[-1]}", flush=True)
            return
        _t.sleep(0.2)
    print(f"[await] TIMEOUT no FileReceived/BadChecksum in {secs}s", flush=True)


def test_send_assert(fprime_test_api: IntegrationTestAPI):
    """Send a command and assert the cmd RESPONSE is OK. env CMD, ARGS, TIMEOUT.

    Used to read the GRC command verdict (e.g. CONTINUOUS_WAVE OK vs EXECUTION_ERROR)
    over a port that DOES have a control downlink (the GRC console GDS).
    """
    import os as _os

    cmd = _os.environ["CMD"]
    raw = _os.environ.get("ARGS", "")
    args = [a for a in raw.split(",") if a != ""]
    timeout = int(_os.environ.get("TIMEOUT", "15"))
    fprime_test_api.send_and_assert_command(
        cmd, args=args, max_delay=timeout, timeout=timeout
    )


def test_listen_events(fprime_test_api: IntegrationTestAPI):
    """Passively collect decoded events for LISTEN_SECS and print them.

    Run against the FC-direct GDS (USB) to observe the flight's verdict for a
    command injected over the radio via the bridge GDS. Independent of LoRa
    downlink / flight TX state.
    """
    import os as _os
    import time as _t

    secs = int(_os.environ.get("LISTEN_SECS", "30"))
    fprime_test_api.clear_histories()
    print(f"[listen] collecting events for {secs}s ...")
    _t.sleep(secs)
    hist = fprime_test_api.get_event_test_history()
    items = list(hist.retrieve())
    print(f"[listen] {len(items)} events:")
    for it in items:
        print(
            "EVT",
            it.get_time(),
            it.template.get_full_name(),
            "|",
            it.get_display_text(),
        )
    tlm_hist = fprime_test_api.get_telemetry_test_history()
    tlm_items = list(tlm_hist.retrieve())
    print(f"[listen] {len(tlm_items)} telemetry updates:")
    for it in tlm_items:
        print(
            "TLM",
            it.get_time(),
            it.template.get_full_name(),
            "=",
            it.get_display_text(),
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


def test_spray_mixed_seq_noop(fprime_test_api: IntegrationTestAPI):
    """Interleave bypass GET_SEQ_NUM (window detector) and NO_OP (the goal).

    Whenever a flight RX window opens, both are in flight: GET_SEQ_NUM emits
    EmitSequenceNumber (proves the window/link is open + reports auth `expected`),
    and NO_OP lands NoOpReceived IFF the GDS framer seq is in window. Lets us
    distinguish "link dark" (neither lands) from "seq out of window" (only
    GET_SEQ_NUM lands) in a single run. env REPS (pairs), GAP.
    """
    import time as _t

    reps = int(os.environ.get("REPS", "120"))
    gap = float(os.environ.get("GAP", "0.4"))
    for i in range(reps):
        fprime_test_api.send_command("ComCcsdsLora.authenticatelora.GET_SEQ_NUM")
        _t.sleep(gap)
        fprime_test_api.send_command("CdhCore.cmdDisp.CMD_NO_OP")
        _t.sleep(gap)
    print(f"[mixed] sent {reps} (GET_SEQ_NUM + NO_OP) pairs @ {gap}s")
    _t.sleep(3)


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
    import time as _t

    print(f"UPSTART {_t.time():.3f}", flush=True)
    try:
        fprime_test_api.uplink_file_and_await_completion(src, dest, timeout=timeout)
    except Exception as e:
        print(f"[uplink] await ended: {type(e).__name__} (expected in TX-off mode)")
    print(f"UPEND {_t.time():.3f}", flush=True)
    print(f"[uplink] send phase done for {dest}")
