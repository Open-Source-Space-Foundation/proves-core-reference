"""#601 bench measurement. Runs under pytest on the bench Pi against a live GDS.

Env: BENCH_CONDITION=idle|load, BENCH_DURATION_S (default 300), BENCH_OUT (output text file).
Flow: link check, REPORT (boot-phase probe stats), RESET_PROBES, run the condition, REPORT,
then dump every schedProbe event and the last value of the channels of interest.
"""
import os
import time

CONDITION = os.environ.get("BENCH_CONDITION", "idle")
DURATION = int(os.environ.get("BENCH_DURATION_S", "300"))
OUT = os.environ.get("BENCH_OUT", f"/tmp/bench601-{CONDITION}.txt")
CMD_PERIOD_S = float(os.environ.get("BENCH_CMD_PERIOD_S", "0.5"))

CHANNELS = [
    "ReferenceDeployment.rateGroup50Hz.RgMaxTime", "ReferenceDeployment.rateGroup10Hz.RgMaxTime", "ReferenceDeployment.rateGroup1Hz.RgMaxTime",
    "ReferenceDeployment.rateGroup50Hz.RgCycleSlips", "ReferenceDeployment.rateGroup10Hz.RgCycleSlips", "ReferenceDeployment.rateGroup1Hz.RgCycleSlips",
    "ReferenceDeployment.startupManager.BootCount", "ReferenceDeployment.startupManager.QuiescenceEndTime", "ReferenceDeployment.modeManager.CurrentMode",
    "ReferenceDeployment.uspRadio.RxProfile", "ReferenceDeployment.uspRadio.TxProfile", "ReferenceDeployment.uspRadio.BytesReceived", "ReferenceDeployment.uspRadio.RxDropped",
    "CdhCore.cmdDisp.CommandsDispatched", "CdhCore.cmdDisp.CommandsDropped",
    "CdhCore.version.ProjectVersion", "ReferenceDeployment.fsSpace.FreeSpace", "ReferenceDeployment.fsSpace.TotalSpace",
    "ComCcsdsUart.comQueue.comQueueDepth",
]

# Rotating command mix: commanding, telemetry/event bursts, sensor reads, file system work.
LOAD_COMMANDS = [
    ("CdhCore.cmdDisp.CMD_NO_OP", []),
    ("CdhCore.cmdDisp.CMD_NO_OP_STRING", ["bench-601 synthetic load 0123456789"]),
    ("ReferenceDeployment.modeManager.GET_CURRENT_MODE", []),
    ("ReferenceDeployment.imuManager.GET_ACCELERATION", []),
    ("ReferenceDeployment.imuManager.GET_MAGNETIC_FIELD", []),
    ("ReferenceDeployment.powerMonitor.GET_TOTAL_POWER", []),
    ("CdhCore.tlmSend.SEND_PKT", [2, "REALTIME"]),
    ("CdhCore.events.DUMP_FILTER_STATE", []),
    ("FileHandling.fileManager.ListDirectory", ["/"]),
    ("FileHandling.fileManager.FileSize", ["/boot_count.bin"]),
    ("FileHandling.fileManager.CalculateCrc", ["/boot_count.bin"]),
    ("FileHandling.fileManager.CreateDirectory", ["/bench601"]),
    ("FileHandling.fileManager.RemoveDirectory", ["/bench601"]),
    ("FileHandling.fileDownlink.SendFile", ["/boot_count.bin", "boot_count.bin"]),
    ("CdhCore.version.VERSION", ["ALL"]),
    ("ReferenceDeployment.startupManager.GET_BOOT_COUNT", []),
]
MODE_TOGGLE_PERIOD_S = 60.0
MODE_SAFE_HOLD_S = 15.0


def _send(api, name, args, stats, timeout=6):
    try:
        api.send_and_assert_command(name, args, timeout=timeout)
        stats["ok"] += 1
    except Exception as exc:  # keep loading; record the failure
        stats["fail"] += 1
        stats.setdefault("failures", []).append(f"{name} {args}: {type(exc).__name__}: {str(exc)[:120]}")


def run_load(api, duration_s, stats):
    t_end = time.time() + duration_s
    i = 0
    next_mode = time.time() + MODE_TOGGLE_PERIOD_S
    in_safe_since = None
    while time.time() < t_end:
        now = time.time()
        if in_safe_since is None and now >= next_mode:
            _send(api, "ReferenceDeployment.modeManager.FORCE_SAFE_MODE", [], stats, timeout=15)
            in_safe_since = now
        elif in_safe_since is not None and now - in_safe_since >= MODE_SAFE_HOLD_S:
            _send(api, "ReferenceDeployment.modeManager.EXIT_SAFE_MODE", [], stats, timeout=15)
            in_safe_since = None
            next_mode = now + MODE_TOGGLE_PERIOD_S
        name, args = LOAD_COMMANDS[i % len(LOAD_COMMANDS)]
        i += 1
        _send(api, name, args, stats)
        time.sleep(CMD_PERIOD_S)
    if in_safe_since is not None:
        _send(api, "ReferenceDeployment.modeManager.EXIT_SAFE_MODE", [], stats, timeout=15)


def report(api, out, label):
    api.send_and_assert_command("ReferenceDeployment.schedProbe.REPORT", timeout=10)
    api.await_event("ReferenceDeployment.schedProbe.ReportDone", timeout=60)
    out.write(f"\n== {label} REPORT at {time.strftime('%Y-%m-%dT%H:%M:%S')} ==\n")


def dump(api, out):
    out.write("\n-- schedProbe events --\n")
    for ev in api.get_event_test_history().retrieve():
        name = ev.template.get_full_name()
        if name.startswith("ReferenceDeployment.schedProbe."):
            out.write(f"{ev.time.to_readable()} {name} {ev.get_display_text()}\n")
    out.write("\n-- other events (count by name) --\n")
    counts = {}
    for ev in api.get_event_test_history().retrieve():
        name = ev.template.get_full_name()
        if not name.startswith("ReferenceDeployment.schedProbe."):
            counts[name] = counts.get(name, 0) + 1
    for name in sorted(counts):
        out.write(f"{counts[name]:6d} {name}\n")
    out.write("\n-- last channel values --\n")
    last = {}
    first = {}
    for ch in api.get_telemetry_test_history().retrieve():
        name = ch.template.get_full_name()
        last[name] = ch
        first.setdefault(name, ch)
    for name in CHANNELS:
        if name in last:
            out.write(f"{name} = {last[name].get_val()} (first seen {first[name].get_val()} at {first[name].time.to_readable()}, last at {last[name].time.to_readable()})\n")
        else:
            out.write(f"{name} = <not received>\n")
    out.write("\n-- RgMaxTime history --\n")
    for ch in api.get_telemetry_test_history().retrieve():
        name = ch.template.get_full_name()
        if name.endswith("RgMaxTime") or name.endswith("RgCycleSlips"):
            out.write(f"{ch.time.to_readable()} {name} = {ch.get_val()}\n")


def test_bench_601(fprime_test_api):
    api = fprime_test_api
    stats = {"ok": 0, "fail": 0}
    with open(OUT, "w") as out:
        out.write(f"condition={CONDITION} duration_s={DURATION} cmd_period_s={CMD_PERIOD_S} start={time.strftime('%Y-%m-%dT%H:%M:%S')}\n")
        api.send_and_assert_command("CdhCore.cmdDisp.CMD_NO_OP", timeout=10)
        report(api, out, "boot-phase")
        api.send_and_assert_command("ReferenceDeployment.schedProbe.RESET_PROBES", timeout=10)
        t0 = time.time()
        if CONDITION == "load":
            run_load(api, DURATION, stats)
        else:
            time.sleep(DURATION)
        elapsed = time.time() - t0
        out.write(f"condition ran {elapsed:.1f} s; commands ok={stats['ok']} fail={stats['fail']}\n")
        for f in stats.get("failures", [])[:40]:
            out.write(f"  FAIL {f}\n")
        report(api, out, "end-of-condition")
        time.sleep(2)
        dump(api, out)
    assert stats["fail"] <= stats["ok"], "more command failures than successes; link unhealthy"
