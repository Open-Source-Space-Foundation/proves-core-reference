# """
# burnwire_test.py:

# Integration tests for the Burnwire component.
# """

# import pytest
# from fprime_gds.common.testing_fw.api import IntegrationTestAPI
# from fprime_gds.common.testing_fw.predicates import event_predicate
# from wdk import burnwire


# @pytest.fixture(autouse=True)
# def reset_burnwire(fprime_test_api: IntegrationTestAPI, start_gds):
#     """Fixture to stop burnwire and clear histories before/after each test"""
#     # Stop burnwire and clear before test
#     stop_burnwire(fprime_test_api)
#     yield
#     # Clear again after test to prevent residue
#     stop_burnwire(fprime_test_api)


# def stop_burnwire(fprime_test_api: IntegrationTestAPI):
#     """Stop the burnwire and clear histories"""

#     # List of events we expect to see
#     events: list[event_predicate] = [
#         fprime_test_api.get_event_pred(f"{burnwire}.SetBurnwireState", "OFF"),
#         fprime_test_api.get_event_pred(f"{burnwire}.BurnwireEndCount"),
#     ]
#     fprime_test_api.send_and_assert_event(
#         f"{burnwire}.STOP_BURNWIRE", events=events, timeout=10
#     )


# def test_01_start_and_stop_burnwire(fprime_test_api: IntegrationTestAPI, start_gds):
#     """Test that burnwire starts and stops as expected"""

#     events: list[event_predicate] = [
#         fprime_test_api.get_event_pred(f"{burnwire}.SetBurnwireState", "ON"),
#         fprime_test_api.get_event_pred(f"{burnwire}.SafetyTimerState"),
#         fprime_test_api.get_event_pred(f"{burnwire}.SetBurnwireState", "OFF"),
#         fprime_test_api.get_event_pred(f"{burnwire}.BurnwireEndCount"),
#     ]

#     # Start burnwire
#     fprime_test_api.send_and_assert_event(
#         f"{burnwire}.START_BURNWIRE", events=events, timeout=20
#     )


# def test_02_manual_stop_before_timeout(fprime_test_api: IntegrationTestAPI, start_gds):
#     """Test that burnwire stops manually before the safety timer expires"""

#     # Start burnwire
#     fprime_test_api.send_and_assert_event(
#         f"{burnwire}.START_BURNWIRE",
#         events=[
#             fprime_test_api.get_event_pred(f"{burnwire}.SetBurnwireState", "ON"),
#         ],
#         timeout=10,
#     )

#     # Stop burnwire before safety timer triggers
#     fprime_test_api.send_and_assert_event(
#         f"{burnwire}.STOP_BURNWIRE",
#         events=[
#             fprime_test_api.get_event_pred(f"{burnwire}.SetBurnwireState", "OFF"),
#             fprime_test_api.get_event_pred(f"{burnwire}.BurnwireEndCount"),
#         ],
#         timeout=10,
#     )
