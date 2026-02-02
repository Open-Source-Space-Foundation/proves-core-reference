#!/usr/bin/env python3
"""
Flaky Test Runner for Integration Tests

Runs integration tests multiple times to detect non-reproducible failures.
Useful for debugging race conditions, hardware timing issues, and intermittent bugs.

Usage:
    # Run all tests 10 times
    python3 FprimeZephyrReference/test/int/run_flaky_tests.py --iterations 10

    # Run specific test 20 times
    python3 FprimeZephyrReference/test/int/run_flaky_tests.py --test antenna_deployer_test --iterations 20

    # Run specific test method 50 times
    python3 FprimeZephyrReference/test/int/run_flaky_tests.py --test antenna_deployer_test::test_deploy_antenna --iterations 50

    # Run with verbose pytest output
    python3 FprimeZephyrReference/test/int/run_flaky_tests.py --test rtc_test --iterations 10 --verbose

    # Stop on first failure (useful for debugging)
    python3 FprimeZephyrReference/test/int/run_flaky_tests.py --test power_monitor_test --iterations 100 --stop-on-failure
"""

import argparse
import json
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import List, Optional


class Colors:
    """ANSI color codes for terminal output"""

    HEADER = "\033[95m"
    OKBLUE = "\033[94m"
    OKCYAN = "\033[96m"
    OKGREEN = "\033[92m"
    WARNING = "\033[93m"
    FAIL = "\033[91m"
    ENDC = "\033[0m"
    BOLD = "\033[1m"
    UNDERLINE = "\033[4m"


class TestResult:
    """Represents a single test run result"""

    def __init__(self, iteration: int, success: bool, duration: float, output: str):
        self.iteration = iteration
        self.success = success
        self.duration = duration
        self.output = output
        self.timestamp = datetime.now()


class TestRunner:
    """Manages multiple test runs and collects results"""

    def __init__(
        self,
        test_name: Optional[str],
        iterations: int,
        verbose: bool,
        stop_on_failure: bool,
        deployment_path: str,
        timeout: int = 120,
    ):
        self.test_name = test_name
        self.iterations = iterations
        self.verbose = verbose
        self.stop_on_failure = stop_on_failure
        self.deployment_path = deployment_path
        self.timeout = timeout
        self.results: List[TestResult] = []
        self.log_dir = Path("logs/flaky_tests")
        self.log_dir.mkdir(parents=True, exist_ok=True)

    def build_pytest_command(self) -> List[str]:
        """Build the pytest command to execute"""
        # Use python -m pytest to ensure we use the venv's pytest
        cmd = [sys.executable, "-m", "pytest"]

        # Get the directory containing this script
        script_dir = Path(__file__).parent

        # Test path (relative to script directory)
        if self.test_name:
            # Check if it's a specific test method (contains ::)
            if "::" in self.test_name:
                test_path = str(script_dir / self.test_name)
            elif self.test_name.endswith(".py"):
                test_path = str(script_dir / self.test_name)
            else:
                test_path = str(script_dir / f"{self.test_name}.py")
        else:
            test_path = str(script_dir)

        cmd.append(test_path)

        # Deployment path
        cmd.extend(["--deployment", self.deployment_path])

        # Verbosity
        if self.verbose:
            cmd.append("-v")
        else:
            cmd.append("-q")

        # Disable warnings for cleaner output
        cmd.append("--disable-warnings")

        # Show all test output (even on success) if verbose
        if self.verbose:
            cmd.append("-s")

        return cmd

    def run_single_iteration(self, iteration: int) -> TestResult:
        """Run a single test iteration"""
        start_time = time.time()
        cmd = self.build_pytest_command()

        # Show command being run
        cmd_str = " ".join(cmd)
        if len(cmd_str) > 120:
            # Truncate long commands for readability
            display_cmd = cmd_str[:117] + "..."
        else:
            display_cmd = cmd_str
        print(
            f"\n{Colors.OKCYAN}[{iteration}/{self.iterations}]{Colors.ENDC} {display_cmd}"
        )

        try:
            result = subprocess.run(
                cmd, capture_output=True, text=True, timeout=self.timeout
            )
            duration = time.time() - start_time
            success = result.returncode == 0
            output = result.stdout + result.stderr

            # Print immediate feedback
            if success:
                print(f"{Colors.OKGREEN}✓ PASS{Colors.ENDC} ({duration:.2f}s)")
            else:
                print(f"{Colors.FAIL}✗ FAIL{Colors.ENDC} ({duration:.2f}s)")
                if self.verbose:
                    print(f"\n{Colors.WARNING}Output:{Colors.ENDC}")
                    print(output)

            return TestResult(iteration, success, duration, output)

        except subprocess.TimeoutExpired:
            duration = time.time() - start_time
            print(
                f"{Colors.FAIL}✗ TIMEOUT{Colors.ENDC} ({duration:.2f}s) - exceeded {self.timeout}s limit"
            )
            return TestResult(
                iteration,
                False,
                duration,
                f"Test execution timed out after {self.timeout} seconds",
            )
        except Exception as e:
            duration = time.time() - start_time
            print(f"{Colors.FAIL}✗ ERROR{Colors.ENDC} ({duration:.2f}s): {e}")
            return TestResult(iteration, False, duration, f"Error running test: {e}")

    def save_failure_log(self, result: TestResult):
        """Save detailed failure logs"""
        timestamp = result.timestamp.strftime("%Y%m%d_%H%M%S")
        test_name = self.test_name or "all_tests"
        log_file = (
            self.log_dir / f"{test_name}_failure_iter{result.iteration}_{timestamp}.log"
        )

        with open(log_file, "w") as f:
            f.write(f"Test: {test_name}\n")
            f.write(f"Iteration: {result.iteration}\n")
            f.write(f"Timestamp: {result.timestamp}\n")
            f.write(f"Duration: {result.duration:.2f}s\n")
            f.write(f"\n{'=' * 80}\n")
            f.write("OUTPUT:\n")
            f.write(f"{'=' * 80}\n\n")
            f.write(result.output)

        return log_file

    def run_all_iterations(self):
        """Run all test iterations"""
        print(f"\n{Colors.BOLD}{Colors.HEADER}Starting Flaky Test Runner{Colors.ENDC}")
        print(f"Test: {self.test_name or 'All integration tests'}")
        print(f"Iterations: {self.iterations}")
        print(f"Stop on failure: {self.stop_on_failure}")
        print(f"Log directory: {self.log_dir}")
        print(f"{'=' * 80}\n")

        for i in range(1, self.iterations + 1):
            result = self.run_single_iteration(i)
            self.results.append(result)

            if not result.success:
                log_file = self.save_failure_log(result)
                print(f"{Colors.WARNING}Failure log saved to: {log_file}{Colors.ENDC}")

                if self.stop_on_failure:
                    print(
                        f"\n{Colors.FAIL}Stopping on first failure (iteration {i}){Colors.ENDC}"
                    )
                    break

    def print_summary(self):
        """Print summary statistics"""
        total = len(self.results)
        passed = sum(1 for r in self.results if r.success)
        failed = total - passed
        success_rate = (passed / total * 100) if total > 0 else 0

        # Calculate duration stats
        durations = [r.duration for r in self.results]
        avg_duration = sum(durations) / len(durations) if durations else 0
        min_duration = min(durations) if durations else 0
        max_duration = max(durations) if durations else 0

        print(f"\n{'=' * 80}")
        print(f"{Colors.BOLD}{Colors.HEADER}TEST SUMMARY{Colors.ENDC}")
        print(f"{'=' * 80}\n")

        print(f"Test: {self.test_name or 'All integration tests'}")
        print(f"Total iterations: {total}")
        print(f"Passed: {Colors.OKGREEN}{passed}{Colors.ENDC}")
        print(f"Failed: {Colors.FAIL}{failed}{Colors.ENDC}")

        # Color-coded success rate
        if success_rate == 100:
            rate_color = Colors.OKGREEN
        elif success_rate >= 80:
            rate_color = Colors.WARNING
        else:
            rate_color = Colors.FAIL

        print(f"Success rate: {rate_color}{success_rate:.1f}%{Colors.ENDC}")

        print("\nDuration statistics:")
        print(f"  Average: {avg_duration:.2f}s")
        print(f"  Min: {min_duration:.2f}s")
        print(f"  Max: {max_duration:.2f}s")

        # Show failure iterations
        if failed > 0:
            failure_iters = [r.iteration for r in self.results if not r.success]
            print(f"\n{Colors.FAIL}Failed iterations:{Colors.ENDC} {failure_iters}")
            print(f"\nFailure logs saved in: {self.log_dir}")

        print(f"\n{'=' * 80}\n")

        # Return exit code based on results
        return 0 if failed == 0 else 1

    def save_json_report(self):
        """Save detailed JSON report"""
        report = {
            "test_name": self.test_name or "all_tests",
            "iterations": self.iterations,
            "timestamp": datetime.now().isoformat(),
            "summary": {
                "total": len(self.results),
                "passed": sum(1 for r in self.results if r.success),
                "failed": sum(1 for r in self.results if not r.success),
                "success_rate": (
                    sum(1 for r in self.results if r.success) / len(self.results) * 100
                    if self.results
                    else 0
                ),
            },
            "results": [
                {
                    "iteration": r.iteration,
                    "success": r.success,
                    "duration": r.duration,
                    "timestamp": r.timestamp.isoformat(),
                }
                for r in self.results
            ],
        }

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        test_name = self.test_name or "all_tests"
        report_file = self.log_dir / f"{test_name}_report_{timestamp}.json"

        with open(report_file, "w") as f:
            json.dump(report, f, indent=2)

        print(f"JSON report saved to: {report_file}")
        return report_file


def main():
    parser = argparse.ArgumentParser(
        description="Run integration tests multiple times to detect flaky behavior",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Run all tests 10 times
  %(prog)s --iterations 10

  # Run specific test 20 times
  %(prog)s --test antenna_deployer_test --iterations 20

  # Run specific test method 50 times
  %(prog)s --test rtc_test::test_set_time --iterations 50

  # Run with verbose output and stop on first failure
  %(prog)s --test power_monitor_test --iterations 100 --verbose --stop-on-failure
        """,
    )

    parser.add_argument(
        "--test",
        "-t",
        help="Specific test file or test method to run (e.g., 'antenna_deployer_test' or 'rtc_test::test_set_time')",
    )

    parser.add_argument(
        "--known-flaky",
        "-k",
        action="store_true",
        help="Run all known flaky tests from issue #138 (antenna_deployer, power_monitor, rtc, imu_manager)",
    )

    parser.add_argument(
        "--iterations",
        "-i",
        type=int,
        default=10,
        help="Number of times to run the test(s) (default: 10)",
    )

    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Enable verbose pytest output",
    )

    parser.add_argument(
        "--stop-on-failure",
        "-s",
        action="store_true",
        help="Stop running on first test failure",
    )

    parser.add_argument(
        "--deployment",
        "-d",
        default="build-artifacts/zephyr/fprime-zephyr-deployment",
        help="Path to deployment directory (default: build-artifacts/zephyr/fprime-zephyr-deployment)",
    )

    parser.add_argument(
        "--timeout",
        type=int,
        default=120,
        help="Timeout in seconds for each test run (default: 120)",
    )

    args = parser.parse_args()

    # Check for conflicting arguments
    if args.test and args.known_flaky:
        print(
            f"{Colors.FAIL}Error: Cannot use --test and --known-flaky together{Colors.ENDC}"
        )
        return 1

    # Validate deployment path exists
    if not Path(args.deployment).exists():
        print(
            f"{Colors.FAIL}Error: Deployment path does not exist: {args.deployment}{Colors.ENDC}"
        )
        print(
            f"\n{Colors.WARNING}Make sure you've built the firmware first with: make build{Colors.ENDC}"
        )
        return 1

    # Check if GDS should be stopped (warn user)
    print(f"\n{Colors.WARNING}⚠ IMPORTANT:{Colors.ENDC}")
    print("Integration tests automatically start their own GDS instance.")
    print(
        f"{Colors.FAIL}STOP any manually running GDS to avoid conflicts!{Colors.ENDC}"
    )
    print("\nMake sure:")
    print(f"  1. {Colors.OKGREEN}Board is connected via USB{Colors.ENDC}")
    print(f"  2. {Colors.OKGREEN}Firmware is flashed and running{Colors.ENDC}")
    print(
        f"  3. {Colors.FAIL}No GDS is running manually{Colors.ENDC} (tests start their own)"
    )
    print("\nPress Enter to continue or Ctrl+C to abort...")
    try:
        input()
    except KeyboardInterrupt:
        print("\nAborted by user")
        return 1

    # Handle known flaky tests mode
    if args.known_flaky:
        # Tests mentioned in issue #138
        known_flaky_tests = [
            "antenna_deployer_test",
            "power_monitor_test",
            "rtc_test",
            "imu_manager_test",
        ]

        print(
            f"\n{Colors.BOLD}{Colors.HEADER}Running Known Flaky Tests (Issue #138){Colors.ENDC}"
        )
        print(f"Tests: {', '.join(known_flaky_tests)}")
        print(f"Iterations per test: {args.iterations}")
        print(f"{'=' * 80}\n")

        all_results = {}
        overall_exit_code = 0

        for test_name in known_flaky_tests:
            print(f"\n{Colors.BOLD}{Colors.OKCYAN}{'=' * 80}{Colors.ENDC}")
            print(f"{Colors.BOLD}{Colors.OKCYAN}Starting: {test_name}{Colors.ENDC}")
            print(f"{Colors.BOLD}{Colors.OKCYAN}{'=' * 80}{Colors.ENDC}\n")

            runner = TestRunner(
                test_name=test_name,
                iterations=args.iterations,
                verbose=args.verbose,
                stop_on_failure=args.stop_on_failure,
                deployment_path=args.deployment,
                timeout=args.timeout,
            )

            runner.run_all_iterations()
            runner.save_json_report()
            exit_code = runner.print_summary()

            # Store results for final summary
            total = len(runner.results)
            passed = sum(1 for r in runner.results if r.success)
            all_results[test_name] = {
                "total": total,
                "passed": passed,
                "failed": total - passed,
                "success_rate": (passed / total * 100) if total > 0 else 0,
            }

            if exit_code != 0:
                overall_exit_code = 1

            # Add spacing between tests
            print("\n")

        # Print overall summary
        print(f"\n{Colors.BOLD}{Colors.HEADER}{'=' * 80}{Colors.ENDC}")
        print(
            f"{Colors.BOLD}{Colors.HEADER}OVERALL SUMMARY - All Known Flaky Tests{Colors.ENDC}"
        )
        print(f"{Colors.BOLD}{Colors.HEADER}{'=' * 80}{Colors.ENDC}\n")

        for test_name, results in all_results.items():
            rate = results["success_rate"]
            if rate == 100:
                rate_color = Colors.OKGREEN
            elif rate >= 80:
                rate_color = Colors.WARNING
            else:
                rate_color = Colors.FAIL

            print(f"{test_name}:")
            print(
                f"  Passed: {Colors.OKGREEN}{results['passed']}/{results['total']}{Colors.ENDC}"
            )
            print(f"  Success rate: {rate_color}{rate:.1f}%{Colors.ENDC}")

        print(f"\n{Colors.BOLD}Logs saved in: logs/flaky_tests/{Colors.ENDC}")
        print(f"{'=' * 80}\n")

        return overall_exit_code

    # Run single test or all tests
    runner = TestRunner(
        test_name=args.test,
        iterations=args.iterations,
        verbose=args.verbose,
        stop_on_failure=args.stop_on_failure,
        deployment_path=args.deployment,
        timeout=args.timeout,
    )

    runner.run_all_iterations()
    runner.save_json_report()
    return runner.print_summary()


if __name__ == "__main__":
    sys.exit(main())
