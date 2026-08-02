#!/usr/bin/env python3
"""
ota_update.py: Manual OTA image-swap driver for PROVES FSW.

This is the standalone, operator-facing counterpart to the automated integration
test in PROVESFlightControllerReference/test/int/ota_update_test.py. It drives the
same Update.updater command sequence against a running GDS so an image can be
staged, swapped, and confirmed by hand (e.g. on the bench or against a real board).

The reusable pieces — the CRC oracle (`calculate_crc`), the CRC-retry re-uplink
loop (`uplink_until_crc`), and the command sequence (`stage_update`) — are shared
in spirit with the test helpers; keep the two in sync when the Updater interface
changes.

Usage:
    # Start GDS first (in another terminal / background), pointing at the board:
    #   make gds-integration GDS_COMMAND="uv run fprime-gds --file-uplink-cooldown 0.8"
    #
    # Then stage an update (uploads, verifies CRC, prepares, writes, sets TEST boot):
    #   python scripts/ota_update.py --firmware build-artifacts/zephyr.signed.bin
    #
    # After power-cycling and validating the new image, confirm it permanently:
    #   python scripts/ota_update.py --confirm-only
"""

import argparse
import sys
import time
import zlib
from pathlib import Path

sys.path.insert(
    0, str(Path(__file__).parent.parent / "PROVESFlightControllerReference/test/int")
)

from common import proves_send_and_assert_command, set_default_retries  # noqa: E402
from fprime_gds.common.testing_fw.api import IntegrationTestAPI  # noqa: E402
from fprime_gds.executables.cli import StandardPipelineParser  # noqa: E402

UPDATER = "Update.updater"
WORKER = "Update.worker"
FILE_MANAGER = "FileHandling.fileManager"
LORA = "ReferenceDeployment.lora"
DOWNLINK_DELAY = "ReferenceDeployment.downlinkDelay"
RADIO_STABILIZE_S = 15
FILE_UPLINK_TIMEOUT_S = 3600  # 1 hour: full image at ~4KB/s effective LoRa rate
UPLINK_ATTEMPTS = 4


def calculate_crc(file_path: Path) -> int:
    """CRC32 matching FileHandling.fileManager.CalculateCrc (zlib.crc32 ^ 0xFFFFFFFF)."""
    crc = 0
    with open(file_path, "rb") as fh:
        while chunk := fh.read(8192):
            crc = zlib.crc32(chunk, crc)
    return ~crc & 0xFFFFFFFF


def onboard_crc(api: IntegrationTestAPI, dest: str, timeout: float = 60) -> int | None:
    api.send_command(f"{FILE_MANAGER}.CalculateCrc", [dest])
    evt = api.await_event(f"{FILE_MANAGER}.CalculateCrcSucceeded", timeout=timeout)
    return None if evt is None else evt.args[1].val


def uplink_until_crc(
    api: IntegrationTestAPI, firmware: Path, dest: str, expected: int
) -> bool:
    """Re-uplink the whole file to the same dest until the on-board CRC matches.

    Offset writes are idempotent, so re-sending repairs frames dropped over the air.
    """
    for attempt in range(UPLINK_ATTEMPTS):
        print(f"[ota] uplink attempt {attempt + 1}/{UPLINK_ATTEMPTS} -> {dest} ...")
        try:
            api.uplink_file_and_await_completion(
                str(firmware), dest, timeout=FILE_UPLINK_TIMEOUT_S
            )
        except Exception as e:  # noqa: BLE001
            print(f"[ota] uplink completion not confirmed: {e}")
        actual = onboard_crc(api, dest)
        if actual == expected:
            print(f"[ota] CRC verified: 0x{expected:08x}")
            return True
        got = "None" if actual is None else f"0x{actual:08x}"
        print(f"[ota] CRC mismatch (expected 0x{expected:08x}, got {got}) — retrying")
    return False


def connect_gds(deployment: Path, gds_port: str = "50050"):
    dict_path = deployment / "dict" / "ReferenceDeploymentTopologyDictionary.json"
    logs_path = deployment / "logs"
    logs_path.mkdir(parents=True, exist_ok=True)
    cli_args = [
        "--dictionary",
        str(dict_path),
        "--logs",
        str(logs_path),
        "--file-storage-directory",
        str(deployment),
        "--tts-port",
        gds_port,
        "--tts-addr",
        "localhost",
    ]
    parser = StandardPipelineParser()
    args, _, *_ = parser.parse_args(
        StandardPipelineParser.CONSTITUENTS, arguments=cli_args, client=True
    )
    pipeline = parser.pipeline_factory(args)
    api = IntegrationTestAPI(pipeline)
    api.setup()
    return api, pipeline


def enable_radio(api: IntegrationTestAPI):
    print("[radio] DIVIDER_PRM_SET=20 + TRANSMIT=ENABLED (fire-and-forget)...")
    api.send_command(command=f"{DOWNLINK_DELAY}.DIVIDER_PRM_SET", args=[20])
    api.send_command(command=f"{LORA}.TRANSMIT", args=["ENABLED"])


def wait_for_link(api: IntegrationTestAPI, timeout: float = 30.0) -> bool:
    print(f"[link] Waiting up to {timeout}s for link (CMD_NO_OP)...")
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            api.clear_histories()
            api.send_and_assert_command(
                "CdhCore.cmdDisp.CMD_NO_OP", timeout=5, max_delay=5
            )
            print("[link] Link confirmed.")
            return True
        except Exception:
            time.sleep(1)
    print("[link] WARNING: link not confirmed within timeout.")
    return False


def stage_update(api: IntegrationTestAPI, firmware: Path, dest: str, crc: int):
    print("[ota] Step 1/4: uploading image (CRC-retry until match)...")
    if not uplink_until_crc(api, firmware, dest, crc):
        print("[ota] ERROR: image never matched CRC after retries.")
        sys.exit(1)

    print("[ota] Step 2/4: PREPARE_UPDATE...")
    proves_send_and_assert_command(api, f"{UPDATER}.PREPARE_UPDATE", retries=5)
    api.assert_event(f"{UPDATER}.PrepareUpdateSucceeded", timeout=60)

    print(f"[ota] Step 3/4: UPDATE_IMAGE_FROM {dest} crc=0x{crc:08x}...")
    proves_send_and_assert_command(
        api, f"{UPDATER}.UPDATE_IMAGE_FROM", args=[dest, str(crc)], retries=5
    )
    api.assert_event(f"{UPDATER}.UpdateSucceeded", timeout=180)

    print("[ota] Step 4/4: CONFIGURE_NEXT_BOOT TEST (auto-reverts if unconfirmed)...")
    proves_send_and_assert_command(
        api, f"{UPDATER}.CONFIGURE_NEXT_BOOT", args=["TEST"], retries=5
    )
    api.assert_event(f"{UPDATER}.SetNextBoot", timeout=10)

    print("\nUPDATE STAGED — power-cycle the board, validate, then run --confirm-only.")


def confirm_update(api: IntegrationTestAPI):
    print("[confirm] CONFIRM_UPDATE...")
    proves_send_and_assert_command(api, f"{UPDATER}.CONFIRM_UPDATE", retries=5)
    api.assert_event(f"{UPDATER}.ConfirmBoot", timeout=10)
    print("[confirm] Image confirmed as permanent boot image.")


def main():
    set_default_retries(5)
    parser = argparse.ArgumentParser(description="OTA image-swap driver for PROVES FSW")
    parser.add_argument(
        "--firmware", type=Path, default=Path("build-artifacts/zephyr.signed.bin")
    )
    parser.add_argument(
        "--crc", type=str, default=None, help="CRC32 hex; auto-calculated if omitted"
    )
    parser.add_argument(
        "--dest", type=str, default="/ota.bin", help="On-board destination path (8.3)"
    )
    parser.add_argument(
        "--port", type=str, default="50050", help="GDS TTS port (default: 50050)"
    )
    parser.add_argument(
        "--confirm-only", action="store_true", help="Only send CONFIRM_UPDATE"
    )
    parser.add_argument(
        "--skip-radio-enable", action="store_true", help="Do not enable LoRa TRANSMIT"
    )
    args = parser.parse_args()

    deployment = Path("build-artifacts/zephyr/fprime-zephyr-deployment")
    if not deployment.exists():
        print(f"[ERROR] Deployment not found: {deployment}. Run 'make build' first.")
        sys.exit(1)
    if not args.confirm_only and not args.firmware.exists():
        print(f"[ERROR] Firmware not found: {args.firmware}")
        sys.exit(1)

    crc = None
    if not args.confirm_only:
        crc = int(args.crc, 16) if args.crc else calculate_crc(args.firmware)
        print(f"[crc] CRC32 = 0x{crc:08x}")

    api, pipeline = connect_gds(deployment, args.port)
    try:
        if not args.skip_radio_enable:
            enable_radio(api)
        if wait_for_link(api):
            time.sleep(RADIO_STABILIZE_S)
            api.clear_histories()
        if args.confirm_only:
            confirm_update(api)
        else:
            stage_update(api, args.firmware, args.dest, crc)
    except Exception as e:  # noqa: BLE001
        import traceback

        print(f"\n[ERROR] {e}")
        traceback.print_exc()
        sys.exit(1)
    finally:
        try:
            api.teardown()
        except Exception:
            pass
        try:
            pipeline.disconnect()
        except Exception:
            pass


if __name__ == "__main__":
    main()
