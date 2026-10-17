"""
downlink_period_test.py:

Measures the LoRa downlink frame period on the default ComDelay divider. The
ground clock stamps each decoded telemetry packet on arrival; packets that
arrive together belong to one frame.
"""

import itertools
import time

import pytest
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

downlinkDelay = "ReferenceDeployment.downlinkDelay"

DEFAULT_DIVIDER = 299
RATE_GROUP_HZ = 10
EXPECTED_PERIOD_S = (DEFAULT_DIVIDER + 1) / RATE_GROUP_HZ
TOLERANCE_S = 0.2
FRAME_GAP_S = 5.0
FRAMES_NEEDED = 3
DEADLINE_S = EXPECTED_PERIOD_S * (FRAMES_NEEDED + 1)
CI_DIVIDER = 20


class ArrivalClock:
    """Telemetry consumer that records the ground arrival time of every packet."""

    def __init__(self):
        self.arrivals: list[float] = []

    def data_callback(self, data, sender=None):
        self.arrivals.append(time.monotonic())


def frame_starts(arrivals: list[float]) -> list[float]:
    return [
        t for i, t in enumerate(arrivals) if i == 0 or t - arrivals[i - 1] > FRAME_GAP_S
    ]


def test_default_divider_paces_lora_every_30_seconds(
    request: pytest.FixtureRequest, fprime_test_api: IntegrationTestAPI, start_gds
):
    if not request.config.getoption("--with-radio"):
        pytest.skip("measures the LoRa downlink; needs --with-radio")

    coders = fprime_test_api.pipeline.coders
    clock = ArrivalClock()
    fprime_test_api.send_command(f"{downlinkDelay}.DIVIDER_PRM_SET", [DEFAULT_DIVIDER])
    coders.register_channel_consumer(clock)
    try:
        deadline = time.monotonic() + DEADLINE_S
        while (
            time.monotonic() < deadline
            and len(frame_starts(clock.arrivals)) < FRAMES_NEEDED
        ):
            time.sleep(0.5)
    finally:
        coders.remove_channel_consumer(clock)
        fprime_test_api.send_command(f"{downlinkDelay}.DIVIDER_PRM_SET", [CI_DIVIDER])

    starts = frame_starts(clock.arrivals)
    gaps = [b - a for a, b in itertools.pairwise(starts)]
    periods = [gap / max(1, round(gap / EXPECTED_PERIOD_S)) for gap in gaps]
    print(
        f"frame gaps {[round(g, 2) for g in gaps]} -> periods {[round(p, 2) for p in periods]}"
    )

    assert periods, (
        f"too few LoRa frames to measure a period: {len(starts)} frame starts"
    )
    for period in periods:
        assert period == pytest.approx(EXPECTED_PERIOD_S, abs=TOLERANCE_S), (
            f"LoRa frame period {period:.2f} s, expected {EXPECTED_PERIOD_S} s on DIVIDER={DEFAULT_DIVIDER}"
        )
