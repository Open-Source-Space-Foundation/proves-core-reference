"""
generic_device_monitor_test.py:

Integration tests for the Generic Device Monitor component.
"""

from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

# We use the tcaMonitor instance to test the GenericDeviceMonitor component
tcaMonitor = "ReferenceDeployment.tcaMonitor"


def test_01_generic_device_monitor_healthy(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Test that the Generic Device Monitor reports Healthy"""
    result: ChData = fprime_test_api.assert_telemetry(
        f"{tcaMonitor}.Healthy", start="NOW", timeout=5
    )

    assert str(result.get_val()) == "Healthy", (
        f"Generic Device Monitor should be Healthy, got {result.get_val()}"
    )
