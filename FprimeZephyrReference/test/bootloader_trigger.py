from fprime_gds.common.testing_fw.api import IntegrationTestAPI


def test_bootloader(fprime_test_api: IntegrationTestAPI):
    fprime_test_api.send_command(
        "ReferenceDeployment.bootloaderTrigger.TRIGGER_BOOTLOADER"
    )
