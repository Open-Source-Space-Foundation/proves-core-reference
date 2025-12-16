"""
conftest.py:

Pytest configuration for integration tests.

This conftest follows the same pattern as test_day_in_the_life.py,
relying on the fprime_test_api fixture provided by fprime_gds.
"""


def pytest_configure(config):
    """Register custom markers."""
    config.addinivalue_line("markers", "slow: marks tests as slow (5MB file transfers)")
