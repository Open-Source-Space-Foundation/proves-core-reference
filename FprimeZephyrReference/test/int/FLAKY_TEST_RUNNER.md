# Flaky Test Runner

This tool helps detect non-reproducible behavior in integration tests by running them multiple times and tracking failures. This is useful for debugging race conditions, hardware timing issues, and intermittent bugs.

## Related Issue

See [Issue #138](https://github.com/Open-Source-Space-Foundation/proves-core-reference/issues/138) for context on flaky test behavior.

## Known Flaky Tests

Based on Issue #138, the following tests have shown intermittent failures:

- `antenna_deployer_test.py::test_deploy_antenna` (Test 5)
- `power_monitor_test.py` (Tests 1 and 2)
- `rtc_test.py` (Test 3)
- `imu_manager_test.py` (Test 2)

## Prerequisites

1. **Build the firmware:**
   ```bash
   make build
   ```

2. **Flash firmware to board:**
   ```bash
   cp build-artifacts/zephyr.uf2 /Volumes/RPI-RP2
   ```

3. **IMPORTANT: Do NOT start GDS manually!**
   - Integration tests automatically start their own GDS instance
   - If you have GDS running, **stop it first** to avoid conflicts
   - Tests use `make gds-integration` which runs GDS without the GUI

## Usage

### Quick Start - Test All Known Flaky Tests

The easiest way to test all the problematic tests from issue #138 at once:

```bash
# Run all known flaky tests 10 times each (default)
make test-known-flaky

# Run all known flaky tests 50 times each
make test-known-flaky ITERATIONS=50

# Or use the Python script directly
python3 FprimeZephyrReference/test/int/run_flaky_tests.py --known-flaky --iterations 50
```

This will run:
- `antenna_deployer_test.py`
- `power_monitor_test.py`
- `rtc_test.py`
- `imu_manager_test.py`

And provide an overall summary at the end.

### Basic Usage

```bash
# Run all integration tests 10 times
python3 FprimeZephyrReference/test/int/run_flaky_tests.py --iterations 10

# Or use the Makefile target
make test-flaky ITERATIONS=10
```

### Testing Specific Tests

```bash
# Run a specific test file 20 times
python3 FprimeZephyrReference/test/int/run_flaky_tests.py \
  --test antenna_deployer_test --iterations 20

# Run a specific test method 50 times
python3 FprimeZephyrReference/test/int/run_flaky_tests.py \
  --test rtc_test::test_set_time --iterations 50

# Using Makefile
make test-flaky TEST=antenna_deployer_test ITERATIONS=20
```

### Advanced Options

```bash
# Verbose output (shows pytest details)
python3 FprimeZephyrReference/test/int/run_flaky_tests.py \
  --test power_monitor_test --iterations 100 --verbose

# Stop on first failure (useful for debugging)
python3 FprimeZephyrReference/test/int/run_flaky_tests.py \
  --test imu_manager_test --iterations 100 --stop-on-failure

# Custom deployment path
python3 FprimeZephyrReference/test/int/run_flaky_tests.py \
  --deployment path/to/deployment --iterations 10
```

## Output and Logging

### Terminal Output

The script provides real-time feedback:

```
[1/10] Running: pytest FprimeZephyrReference/test/int/rtc_test.py --deployment ...
✓ PASS (2.34s)

[2/10] Running: pytest FprimeZephyrReference/test/int/rtc_test.py --deployment ...
✗ FAIL (3.12s)
```

### Failure Logs

When a test fails, detailed logs are automatically saved to:

```
logs/flaky_tests/<test_name>_failure_iter<N>_<timestamp>.log
```

Example:
```
logs/flaky_tests/rtc_test_failure_iter7_20260202_143052.log
```

### Summary Report

At the end, you get a comprehensive summary:

```
================================================================================
TEST SUMMARY
================================================================================

Test: rtc_test
Total iterations: 100
Passed: 97
Failed: 3
Success rate: 97.0%

Duration statistics:
  Average: 2.45s
  Min: 2.12s
  Max: 4.23s

Failed iterations: [7, 34, 89]

Failure logs saved in: logs/flaky_tests
```

### JSON Report

A machine-readable report is also saved:

```
logs/flaky_tests/<test_name>_report_<timestamp>.json
```

This includes:
- Test name and timestamp
- Summary statistics
- Individual iteration results with durations

## Debugging Workflow

### Step 1: Identify Flaky Tests

Run a test many times to confirm flakiness:

```bash
python3 FprimeZephyrReference/test/int/run_flaky_tests.py \
  --test antenna_deployer_test --iterations 50
```

### Step 2: Stop on First Failure

Run with `--stop-on-failure` to capture the state immediately:

```bash
python3 FprimeZephyrReference/test/int/run_flaky_tests.py \
  --test antenna_deployer_test --iterations 100 --stop-on-failure
```

### Step 3: Analyze Failure Logs

Check the saved logs in `logs/flaky_tests/` for:
- Error messages
- Assertion failures
- Timing issues
- Event/telemetry mismatches

### Step 4: Add Verbose Output

If you need more details:

```bash
python3 FprimeZephyrReference/test/int/run_flaky_tests.py \
  --test antenna_deployer_test --iterations 10 --verbose
```

## Interpreting Results

### Success Rate Guidelines

- **100%**: Test is stable ✓
- **95-99%**: Likely has race conditions or timing issues
- **80-94%**: Significant flakiness - needs investigation
- **<80%**: Critical issues - test or component needs fixing

### Common Causes of Flakiness

1. **Race Conditions**
   - Multiple threads accessing shared state
   - Inadequate synchronization
   - Fix: Add proper locks or synchronization primitives

2. **Hardware Timing Issues**
   - Sensor reading delays
   - I2C/SPI communication timeouts
   - Fix: Increase timeouts or add retry logic

3. **Test Environment State**
   - Files left from previous tests
   - Parameters not reset
   - Fix: Improve test fixtures and cleanup

4. **Event/Telemetry Ordering**
   - Events arrive in unexpected order
   - Telemetry not yet available
   - Fix: Use proper wait conditions instead of fixed delays

## Tips

1. **Run Overnight**: For really flaky tests, run hundreds or thousands of iterations overnight
   ```bash
   nohup python3 FprimeZephyrReference/test/int/run_flaky_tests.py \
     --test problematic_test --iterations 1000 > overnight.log 2>&1 &
   ```

2. **Compare Before/After**: Run flaky tests before and after a fix to verify improvement

3. **Combine with Git Bisect**: Use this tool with git bisect to find when a test became flaky

4. **Log Analysis**: Use `grep` to find patterns in failure logs:
   ```bash
   grep -h "AssertionError" logs/flaky_tests/*.log | sort | uniq -c
   ```

## Makefile Integration

For convenience, you can use the Makefile target:

```bash
# Run with default iterations (10)
make test-flaky

# Specify iterations
make test-flaky ITERATIONS=50

# Specify test and iterations
make test-flaky TEST=rtc_test ITERATIONS=100
```
