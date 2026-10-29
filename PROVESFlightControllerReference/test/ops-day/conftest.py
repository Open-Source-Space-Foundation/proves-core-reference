"""conftest.py:

Fixtures for the ops-day scenario. pytest.ini puts test/int on the import
path, so the scenario reuses test/int/common.py and its sequence-number handling.
"""

import pytest
from fprime_gds.common.testing_fw.api import IntegrationTestAPI
from fprime_gds.common.utils.event_severity import EventSeverity

# The LoRa queue overflows while TRANSMIT is off after a boot. On a flight
# board, startup.seq filters this event, but a fresh image has no
# /seq/startup.bin until the D8 step uplinks it.
# startup.seq filters the rate-group slips for the same reason: they happen on
# loaded hardware, and a shared CI runner is loaded too.
ALWAYS_EXPECTED = {
    "ComCcsdsLora.comQueue.QueueOverflow",
    "ReferenceDeployment.rateGroup50Hz.RateGroupCycleSlip",
    "ReferenceDeployment.rateGroup10Hz.RateGroupCycleSlip",
}


def pytest_configure(config: pytest.Config) -> None:
    config.addinivalue_line(
        "markers",
        "expect_warnings(*events): WARNING_HI events this ops-day step causes on purpose",
    )


@pytest.fixture(scope="module")
def all_events(fprime_test_api_session: IntegrationTestAPI):
    """Every event of the day, kept apart from the histories that each step clears."""
    history = fprime_test_api_session.get_event_subhistory(fsw_order=False)
    yield history
    fprime_test_api_session.remove_event_subhistory(history)


@pytest.fixture(autouse=True)
def no_unexpected_warnings(all_events, request: pytest.FixtureRequest):
    """Fail a step that caused a FATAL, or a WARNING_HI it did not declare."""
    all_events.retrieve_new()
    yield
    marker = request.node.get_closest_marker("expect_warnings")
    expected = ALWAYS_EXPECTED | set(marker.args if marker else ())
    unexpected = [
        event.get_str()
        for event in all_events.retrieve_new()
        if event.get_severity() == EventSeverity.FATAL
        or (
            event.get_severity() == EventSeverity.WARNING_HI
            and event.get_template().get_full_name() not in expected
        )
    ]
    assert not unexpected, "unexpected severe events:\n" + "\n".join(unexpected)
