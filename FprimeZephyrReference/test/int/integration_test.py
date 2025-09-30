"""
integration_test.py:

Simple integration tests for the Watchdog component.
Tests are ordered so that stop tests run last.
"""

def get_watchdog_transitions(fprime_test_api):
    """Helper function to request packet and get fresh WatchdogTransitions telemetry"""
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command("CdhCore.tlmSend.SEND_PKT", ["5"], max_delay=2)
    result = fprime_test_api.assert_telemetry(
        "ReferenceDeployment.watchdog.WatchdogTransitions",
        start="NOW", timeout=3
    )
    return result.get_val()


def test_01_watchdog_telemetry_basic(fprime_test_api):
    """Test that we can read WatchdogTransitions telemetry"""
    value = get_watchdog_transitions(fprime_test_api)
    assert value >= 0, f"WatchdogTransitions should be >= 0, got {value}"


def test_02_watchdog_increments(fprime_test_api):
    """Test that WatchdogTransitions increments over time"""
    import time

    initial_value = get_watchdog_transitions(fprime_test_api)
    time.sleep(2.0)  # Wait for watchdog to run more cycles
    updated_value = get_watchdog_transitions(fprime_test_api)

    assert updated_value > initial_value, \
        f"WatchdogTransitions should increase. Initial: {initial_value}, Updated: {updated_value}"


def test_03_stop_watchdog_command(fprime_test_api):
    """Test TEST_STOP_WATCHDOG command sends and emits WatchdogStop event"""
    fprime_test_api.clear_histories()

    fprime_test_api.send_and_assert_command(
        "ReferenceDeployment.watchdog.TEST_STOP_WATCHDOG",
        max_delay=2
    )

    fprime_test_api.assert_event(
        "ReferenceDeployment.watchdog.WatchdogStop",
        timeout=2
    )


def test_04_watchdog_stops_incrementing(fprime_test_api):
    """Test that WatchdogTransitions stops incrementing after TEST_STOP_WATCHDOG"""
    import time

    # Get initial value (should be from stopped watchdog from previous test)
    initial_value = get_watchdog_transitions(fprime_test_api)

    # Wait and check that it's not incrementing (watchdog should already be stopped)
    time.sleep(2.0)
    final_value = get_watchdog_transitions(fprime_test_api)

    assert final_value == initial_value, \
        f"Watchdog should remain stopped. Initial: {initial_value}, Final: {final_value}"
