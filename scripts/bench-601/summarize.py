#!/usr/bin/env python3
"""Condense a measure_test.py output file into one table per REPORT (probe lateness, handler max-holds,
named threads, CPU) plus the channel values. Usage: summarize.py <condition.txt> [...]"""
import re
import sys

ACTCOMP = {  # Os::Task::getNumTasks() at start() in the autocoded startTasks order (FW_OBJECT_NAMES=0)
    0: "CdhCore.cmdDisp", 1: "CdhCore.events", 2: "CdhCore.tlmSend", 3: "ComCcsdsLora.aggregator",
    4: "ComCcsdsLora.comQueue", 5: "ComCcsdsUart.aggregator", 6: "ComCcsdsUart.comQueue",
    7: "FileHandling.fileDownlink", 8: "FileHandling.fileManager", 9: "FileHandling.fileUplink",
    10: "FileHandling.prmDb", 11: "cmdSeq", 12: "modeManager", 13: "payload", 14: "payloadSeq",
    15: "rateGroup10Hz", 16: "rateGroup1Hz", 17: "rateGroup50Hz", 18: "safeModeSeq", 19: "uspRadio",
    20: "Update.updater", 21: "Update.worker",
}


def name_of(raw):
    m = re.match(r"ActComp_(\d+)$", raw)
    return ACTCOMP.get(int(m.group(1)), raw) if m else raw


def main(path):
    text = open(path).read()
    print(f"### {path}")
    print(text.splitlines()[0])
    for line in text.splitlines():
        if line.startswith("condition ran") or line.startswith("  FAIL"):
            print(line)
    reports = re.split(r"\n== (.*?) REPORT at (\S+) ==\n", text)
    events = [l for l in text.splitlines() if ".schedProbe." in l]
    blocks, cur = [], []
    for l in events:
        cur.append(l)
        if "ReportDone" in l:
            blocks.append(cur)
            cur = []
    labels = ["boot-phase", "end-of-condition"]
    for i, blk in enumerate(blocks):
        print(f"\n--- report {i} ({labels[i] if i < len(labels) else '?'}) ---")
        print(f"{'probe prio':>10} {'wakes':>7} {'max late ms':>12} {'>1ms':>6} {'>5ms':>6} {'>20ms':>6}")
        for l in blk:
            m = re.search(r"probe prio (\d+) period \d+ us: (\d+) wakes, max late (\d+) us, >1ms (\d+), >5ms (\d+), >20ms (\d+)", l)
            if m:
                p, w, ml, a, b, c = m.groups()
                print(f"{p:>10} {w:>7} {int(ml)/1000:>12.1f} {a:>6} {b:>6} {c:>6}")
        for l in blk:
            m = re.search(r"handler (\S+): max (\d+) us over (\d+) calls", l)
            if m:
                print(f"handler {m.group(1):<18} max {int(m.group(2))/1000:6.2f} ms over {m.group(3)} calls")
        print(f"{'thread':<28} {'prio':>4} {'peak us':>8} {'avg us':>7} {'total ms':>9} {'cpu permille':>12}")
        rows = []
        for l in blk:
            m = re.search(r"thread (\S+) prio (-?\d+): peak run (\d+) us, avg run (\d+) us, total (\d+) ms, (\d+) permille", l)
            if m:
                rows.append((int(m.group(2)), name_of(m.group(1)), int(m.group(3)), int(m.group(4)), int(m.group(5)), int(m.group(6))))
        for prio, nm, pk, av, tot, pm in sorted(rows):
            print(f"{nm:<28} {prio:>4} {pk:>8} {av:>7} {tot:>9} {pm:>12}")
        for l in blk:
            m = re.search(r"cpu: (.*)", l)
            if m:
                print("cpu:", m.group(1))
        for l in blk:
            m = re.search(r"gap: prio-11 probe woke (\d+) us late; (\d+) us since its previous wake, of which (\d+) us non-idle", l)
            if m:
                print(f"gap capture: woke {int(m.group(1))/1000:.1f} ms late, {int(m.group(2))/1000:.1f} ms since previous wake, {int(m.group(3))/1000:.1f} ms non-idle")
            m = re.search(r"gap\[(\d+)\] (\S+) prio (-?\d+) used (\d+) us", l)
            if m:
                print(f"  gap[{m.group(1)}] {name_of(m.group(2)):<28} prio {m.group(3):>3}  {int(m.group(4))/1000:7.1f} ms")
    print("\n--- channels ---")
    for l in text.splitlines():
        m = re.match(r"(\S+) = (.*?) \(first seen (.*?) at .*", l)
        if m:
            print(f"{m.group(1):<50} {m.group(2):<14} first {m.group(3)}")
    print("\n--- other events ---")
    on = False
    for l in text.splitlines():
        if l.startswith("-- other events"):
            on = True
            continue
        if l.startswith("-- last channel"):
            break
        if on and l.strip():
            print(l)


if __name__ == "__main__":
    for p in sys.argv[1:]:
        main(p)
