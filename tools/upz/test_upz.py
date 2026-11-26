#!/usr/bin/env python3
"""Round-trip, resume and corruption tests: tools/upz/upz.py against the flight decoder.

The flight decoder runs through the upz_expand host harness. Pass its path with
--cli (CMake does), or let the test compile it with g++ (with ASan/UBSan when
the toolchain has them). Set UPZ_EXTRA_FILES to a colon-separated list of real
images (e.g. a built zephyr.signed.bin) to include them.

    python3 tools/upz/test_upz.py [--cli PATH]
"""

import os
import random
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import upz  # noqa: E402

CLI = None
DONE, FAILED, STOPPED = 0, 1, 3


def build_cli(workdir: str) -> str:
    """Compile the upz_expand harness, with sanitizers when available."""
    out = os.path.join(workdir, "upz_expand")
    srcs = [
        os.path.join(HERE, "upz_expand.cpp"),
        os.path.join(
            ROOT,
            "PROVESFlightControllerReference/Components/FileExpander/UpzExpander.cpp",
        ),
    ]
    base = [
        "g++",
        "-std=c++14",
        "-O1",
        "-g",
        "-Wall",
        "-Wextra",
        "-Werror",
        f"-I{ROOT}",
        "-o",
        out,
    ] + srcs
    for extra in (["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []):
        if subprocess.run(base + extra, capture_output=True).returncode == 0:
            return out
    raise RuntimeError("could not build upz_expand")


def read(path: str) -> bytes:
    with open(path, "rb") as f:
        return f.read()


def thumb_like(n: int, seed: int) -> bytes:
    """Pseudo firmware: Thumb-ish halfwords with many BL / B.W calls to a few targets."""
    rng = random.Random(seed)
    targets = [rng.randrange(0, 1 << 20) & ~1 for _ in range(40)]
    out = bytearray()
    while len(out) < n:
        r = rng.random()
        pc = len(out) + 4
        if r < 0.15:
            off = (rng.choice(targets) - pc) & 0x1FFFFFF
            s = (off >> 24) & 1
            j1 = (~(((off >> 23) & 1) ^ s)) & 1
            j2 = (~(((off >> 22) & 1) ^ s)) & 1
            kind = 0xD000 if rng.random() < 0.8 else 0x9000
            out += struct.pack(
                "<HH",
                0xF000 | (s << 10) | ((off >> 12) & 0x3FF),
                kind | (j1 << 13) | (j2 << 11) | ((off >> 1) & 0x7FF),
            )
        elif r < 0.6:
            out += struct.pack(
                "<H",
                rng.choice([0x4770, 0xB510, 0xBD10, 0x2000, 0x6803, 0x4618, 0x3001]),
            )
        else:
            out += struct.pack("<H", rng.randrange(0, 0x10000))
    return bytes(out[:n])


def inputs():
    """Named test inputs covering edge sizes and data kinds."""
    rng = random.Random(1)
    cases = {
        "empty": b"",
        "one": b"\x42",
        "block": thumb_like(256, 2),
        "block_plus": thumb_like(257, 3),
        "zeros_70k": bytes(70000),
        "random_70k": bytes(rng.randrange(256) for _ in range(70000)),
        "text": (b"PROVES Electra compressed uplink " * 4000)[:131073],
        "thumb_200k": thumb_like(200000, 4),
        "chunk_exact": thumb_like(65536 * 2, 5),
    }
    for path in filter(None, os.environ.get("UPZ_EXTRA_FILES", "").split(":")):
        if os.path.exists(path):
            cases[os.path.basename(path)] = open(path, "rb").read()
    return cases


class UpzTest(unittest.TestCase):
    """Ground encoder against the flight decoder."""

    def setUp(self):
        """Make a scratch directory."""
        self.tmp = tempfile.mkdtemp()

    def tearDown(self):
        """Remove the scratch directory."""
        shutil.rmtree(self.tmp, ignore_errors=True)

    def run_cli(self, coded: str, out: str, *args) -> tuple:
        """Run the harness once; fail on sanitizer reports."""
        r = subprocess.run(
            [CLI, coded, out, *map(str, args)],
            capture_output=True,
            text=True,
            timeout=600,
        )
        self.assertNotIn("runtime error", r.stderr)
        self.assertNotIn("AddressSanitizer", r.stderr)
        return r.returncode, r.stdout.strip()

    def write(self, name: str, data: bytes) -> str:
        """Write a scratch file and return its path."""
        path = os.path.join(self.tmp, name)
        with open(path, "wb") as f:
            f.write(data)
        return path

    def expand(self, raw: bytes, blob: bytes, *args):
        """Expand ``blob`` from scratch and assert the output equals ``raw``."""
        coded = self.write("in.upz", blob)
        out = os.path.join(self.tmp, "out.bin")
        if os.path.exists(out):
            os.remove(out)
        rc, line = self.run_cli(coded, out, *args)
        self.assertEqual(rc, DONE, line)
        self.assertEqual(read(out), raw)
        return line

    def test_round_trip(self):
        """Every input, filtered and not, rebuilds exactly."""
        for name, raw in inputs().items():
            for use_filter in (True, False):
                with self.subTest(name=name, filter=use_filter):
                    blob = upz.compress(raw, use_filter)
                    self.assertEqual(upz.decompress(blob), raw)
                    self.expand(raw, blob)

    def test_small_chunks_and_window(self):
        """Other chunk sizes and windows rebuild exactly."""
        raw = thumb_like(150000, 6)
        for chunk_log2, dict_size in (
            (12, 1 << 20),
            (13, 4096),
            (16, 65536),
            (20, 1 << 20),
        ):
            with self.subTest(chunk_log2=chunk_log2, dict_size=dict_size):
                self.expand(raw, upz.compress(raw, True, chunk_log2, dict_size))

    def test_tiny_budgets(self):
        """One byte and one storage call per step still finishes."""
        raw = thumb_like(70000, 7)
        self.expand(raw, upz.compress(raw), "--out-budget", 1, "--io-budget", 1)

    def test_resume_after_random_resets(self):
        """Random stops and reruns converge on the exact output and resume past chunk 0."""
        raw = thumb_like(300000, 8)
        coded = self.write("in.upz", upz.compress(raw))
        out = os.path.join(self.tmp, "out.bin")
        rng = random.Random(9)
        resumed = set()
        for trial in range(6):
            if os.path.exists(out):
                os.remove(out)
            for _ in range(200):
                rc, line = self.run_cli(
                    coded, out, "--stop-after", rng.randrange(1, 120)
                )
                resumed.add(line.split("resumeChunk=")[1].split()[0])
                if rc == DONE:
                    break
                self.assertEqual(rc, STOPPED, line)
            self.assertEqual(rc, DONE, f"trial {trial} never finished")
            self.assertEqual(read(out), raw)
        self.assertGreater(len(resumed), 2, "resets never resumed past chunk 0")

    def test_torn_and_stale_output(self):
        """Leftover output is kept where valid, discarded where not."""
        raw = thumb_like(200000, 10)
        coded = self.write("in.upz", upz.compress(raw))
        out = os.path.join(self.tmp, "out.bin")
        # Correct first chunk, then garbage: resumes at chunk 1
        self.write("out.bin", raw[:65536] + os.urandom(30000))
        rc, line = self.run_cli(coded, out)
        self.assertEqual(rc, DONE, line)
        self.assertIn("resumeChunk=1", line)
        self.assertEqual(read(out), raw)
        # Longer than the target: start over
        self.write("out.bin", raw + b"stale")
        rc, line = self.run_cli(coded, out)
        self.assertEqual(rc, DONE, line)
        self.assertEqual(read(out), raw)
        # Already complete: verifies without decoding
        rc, line = self.run_cli(coded, out)
        self.assertEqual(rc, DONE, line)
        self.assertIn("farReads=0", line)

    def test_storage_upset_is_redone(self):
        # A byte that lands wrong on storage is caught by the final re-read; its chunk is decoded again
        raw = thumb_like(150000, 11)
        blob = upz.compress(raw)
        # 70000: nothing matches back into it, caught at the end; 300: inside the window later chunks read
        for offset in (70000, 300, 65000):
            with self.subTest(offset=offset):
                self.expand(raw, blob, "--flip", offset)

    def test_too_many_chunks_is_refused(self):
        """The ground tool refuses files the board would reject."""
        with self.assertRaises(ValueError):
            upz.compress(bytes(4097 * 1024), True, 12)

    def test_corrupt_input_fails_cleanly(self):
        """Damaged input fails with a status, never a crash or an oversized output."""
        raw = thumb_like(100000, 12)
        blob = upz.compress(raw)
        rng = random.Random(13)
        data_start = upz.parse(blob)["data_start"]
        cases = [
            blob[: len(blob) // 2],
            blob[:-1],
            blob + b"x",
            b"UPZ1" + bytes(40),
            b"",
        ]
        for _ in range(150):
            b = bytearray(blob)
            pos = rng.randrange(data_start if rng.random() < 0.8 else 0, len(b))
            b[pos] ^= 1 << rng.randrange(8)
            cases.append(bytes(b))
        for i, bad in enumerate(cases):
            with self.subTest(case=i):
                coded = self.write("bad.upz", bad)
                out = os.path.join(self.tmp, "bad.out")
                if os.path.exists(out):
                    os.remove(out)
                rc, line = self.run_cli(coded, out)
                self.assertIn(rc, (DONE, FAILED), line)
                if os.path.exists(out):
                    self.assertLessEqual(os.path.getsize(out), len(raw))
                if rc == DONE:  # a flip the CRCs cannot see would be a bug
                    self.assertEqual(read(out), raw)

    def test_filter_is_bijective(self):
        """The branch filter inverts exactly."""
        raw = thumb_like(65536, 14) + bytes(range(256)) * 64
        self.assertEqual(upz.thumb_filter(upz.thumb_filter(raw, True), False), raw)
        self.assertNotEqual(upz.thumb_filter(raw, True), raw)

    def test_header_matches_board_crc_convention(self):
        """Header CRCs are zlib CRC32."""
        raw = thumb_like(5000, 15)
        self.assertEqual(upz.parse(upz.compress(raw))["raw_crc"], zlib.crc32(raw))


if __name__ == "__main__":
    args = sys.argv[1:]
    work = tempfile.mkdtemp()
    if "--cli" in args:
        i = args.index("--cli")
        CLI = os.path.abspath(args[i + 1])
        del args[i : i + 2]
    else:
        CLI = build_cli(work)
    try:
        unittest.main(argv=[sys.argv[0]] + args, verbosity=2)
    finally:
        shutil.rmtree(work, ignore_errors=True)
