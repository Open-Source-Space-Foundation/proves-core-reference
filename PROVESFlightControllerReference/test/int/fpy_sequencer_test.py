"""
fpy_sequencer_test.py:

Runs real Fpy sequences on the FpySequencer instance whose statement table and
stack live in PSRAM. fpy_simple logs, commands and sleeps. fpy_psram also
recurses, loops and reads telemetry, so a pass shows the statement table and
stack work from PSRAM.
"""

import subprocess
import sys
import tempfile
from pathlib import Path

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

fpySeq = "ReferenceDeployment.fpySeq"
FILES = Path(__file__).parent / "files"

SEQUENCES = {
    "fpy_simple": ["fpy: hello from PSRAM", "fpy: after NO_OP and sleep"],
    "fpy_psram": [
        "fpy: fib(15)=610, loop sent 5 NO_OPs",
        "fpy: read rateGroup1Hz.RgMaxTime telemetry",
    ],
}


def compile_fpy(dictionary: str, source: Path, output: Path) -> None:
    """Compile an .fpy source to a sequence binary with the venv's fprime-fpyc."""
    fpyc = Path(sys.executable).parent / "fprime-fpyc"
    subprocess.run(
        [str(fpyc), "-d", dictionary, "-o", str(output), str(source)], check=True
    )


@pytest.mark.parametrize("name", SEQUENCES.keys())
def test_run_fpy_sequence_from_psram(
    fprime_test_api: IntegrationTestAPI, start_gds, name: str
):
    """Uplink and run one sequence, then expect its log events and SequenceDone."""
    destination = f"/{name}.bin"
    with tempfile.TemporaryDirectory() as tempdir:
        binary = Path(tempdir) / f"{name}.bin"
        compile_fpy(
            fprime_test_api.dictionaries.dictionary_path, FILES / f"{name}.fpy", binary
        )
        # FileUplink does not truncate, so a longer old file would leave trailing bytes
        proves_send_and_assert_command(
            fprime_test_api, "FileHandling.fileManager.RemoveFile", [destination, True]
        )
        fprime_test_api.uplink_file(binary, destination)
        fprime_test_api.assert_event("FileReceived", timeout=30)

    fprime_test_api.clear_histories()
    proves_send_and_assert_command(
        fprime_test_api, f"{fpySeq}.RUN", [destination, "NO_BLOCK"]
    )

    logs = [
        fprime_test_api.get_event_pred(f"{fpySeq}.LogActivityHi", [destination, msg])
        for msg in SEQUENCES[name]
    ]
    done = fprime_test_api.get_event_pred(f"{fpySeq}.SequenceDone", [destination])
    fprime_test_api.assert_event_sequence([*logs, done], timeout=120)
