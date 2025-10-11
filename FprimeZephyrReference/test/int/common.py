from fprime_gds.common.testing_fw.api import IntegrationTestAPI


def proves_send_and_assert_command(
    fprime_test_api: IntegrationTestAPI, command: str, args: list[str] = []
):
    fprime_test_api.clear_histories()

    for attempt in range(3):
        try:
            fprime_test_api.send_and_assert_command(
                command,
                args,
                max_delay=10,
            )
            break
        except AssertionError:
            if attempt == 2:
                raise
