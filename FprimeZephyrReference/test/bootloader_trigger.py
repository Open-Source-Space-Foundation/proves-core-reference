"""
bootloader_trigger.py:

This module is responsible for sending the command to trigger the bootloader mode
on the PROVES hardware during integration tests. Once in bootloader mode, the device
can accept firmware updates.
"""

import os
import subprocess
import time

import pytest
from fprime_gds.common.testing_fw.api import IntegrationTestAPI
from int.common import cmdDispatch


@pytest.fixture(scope="session", autouse=True)
def start_gds(fprime_test_api_session: IntegrationTestAPI):
    """Fixture to start GDS

    GDS is used to send the bootloader commands.
    """
    process = subprocess.Popen(["make", "gds-integration"], cwd=os.getcwd())

    gds_working = False
    timeout_time = time.time() + 30
    while time.time() < timeout_time:
        try:
            fprime_test_api_session.send_and_assert_command(
                command=f"{cmdDispatch}.CMD_NO_OP"
            )
            gds_working = True
            break
        except Exception:
            time.sleep(1)
    assert gds_working

    yield
    process.kill()


def test_bootloader(fprime_test_api: IntegrationTestAPI):
    """Trigger bootloader mode on PROVES hardware"""
    # Don't use proves_send_and_assert_command here because we don't expect
    # a response from the bootloader trigger command. The device will reboot
    # into bootloader mode and may not send a command completion event.
    fprime_test_api.send_command(
        "ReferenceDeployment.bootloaderTrigger.TRIGGER_BOOTLOADER"
    )
