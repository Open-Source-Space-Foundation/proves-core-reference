#!/usr/bin/env python3
"""
run_interactive_tests.py:

Interactive integration test runner for PROVES CubeSat.
Automatically discovers integration tests, allows interactive selection,
and executes them for a configurable number of cycles.

Usage:
    # Interactive mode (with cursor navigation)
    ./run_interactive_tests.py

    # CLI mode
    ./run_interactive_tests.py --tests watchdog_test imu_manager_test --cycles 5
    ./run_interactive_tests.py --all --cycles 10
    ./run_interactive_tests.py --tests watchdog_test --cycles 3 --stop-on-fail

Prerequisites:
    - Hardware connected
    - Deployment built: make build
"""

import argparse
import subprocess
import sys
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from pathlib import Path
from typing import Dict, List, Optional

# Try to import simple-term-menu for interactive selection
try:
    from simple_term_menu import TerminalMenu

    HAS_INTERACTIVE = True
except ImportError:
    HAS_INTERACTIVE = False


@dataclass
class TestResult:
    """Result of a single test execution"""

    test_file: str
    cycle: int
    passed: bool
    duration: float
    exit_code: int
    timestamp: datetime


@dataclass
class TestSummary:
    """Summary statistics for a test file"""

    test_file: str
    total_runs: int = 0
    passed: int = 0
    failed: int = 0
    pass_rate: float = 0.0
    avg_duration: float = 0.0
    is_flaky: bool = False
    failed_cycles: List[int] = field(default_factory=list)


class ResultsTracker:
    """Track and aggregate test results"""

    def __init__(self):
        self.results: List[TestResult] = []
        self.start_time: Optional[datetime] = None
        self.end_time: Optional[datetime] = None

    def add(self, result: TestResult):
        """Add a test result"""
        if self.start_time is None:
            self.start_time = result.timestamp
        self.results.append(result)
        self.end_time = datetime.now()

    def get_summary(self) -> Dict[str, TestSummary]:
        """Aggregate results by test file"""
        summaries: Dict[str, TestSummary] = {}

        for result in self.results:
            if result.test_file not in summaries:
                summaries[result.test_file] = TestSummary(test_file=result.test_file)

            summary = summaries[result.test_file]
            summary.total_runs += 1

            if result.passed:
                summary.passed += 1
            else:
                summary.failed += 1
                summary.failed_cycles.append(result.cycle)

        # Calculate statistics
        for summary in summaries.values():
            if summary.total_runs > 0:
                summary.pass_rate = summary.passed / summary.total_runs
                # Test is flaky if it has both passes and failures
                summary.is_flaky = 0 < summary.pass_rate < 1.0

                # Calculate average duration
                durations = [
                    r.duration for r in self.results if r.test_file == summary.test_file
                ]
                summary.avg_duration = (
                    sum(durations) / len(durations) if durations else 0
                )

        return summaries

    def detect_flaky_tests(self) -> List[str]:
        """Find tests with 0% < failure_rate < 100%"""
        summaries = self.get_summary()
        return [name for name, summary in summaries.items() if summary.is_flaky]

    def get_total_duration(self) -> timedelta:
        """Get total execution time"""
        if self.start_time and self.end_time:
            return self.end_time - self.start_time
        return timedelta(0)


def discover_tests(test_dir: Path) -> List[Path]:
    """Find all *_test.py files in integration test directory"""
    if not test_dir.exists():
        return []

    # Find all test files, excluding common.py and conftest.py
    test_files = sorted(
        [
            f
            for f in test_dir.glob("*_test.py")
            if f.name not in ["common.py", "conftest.py"]
        ]
    )

    return test_files


def interactive_select_tests(test_files: List[Path]) -> Optional[List[Path]]:
    """Interactive test selection with arrow keys (requires simple-term-menu)"""
    if not HAS_INTERACTIVE or not sys.stdin.isatty():
        return fallback_select_tests(test_files)

    menu_entries = [f.name for f in test_files]

    terminal_menu = TerminalMenu(
        menu_entries,
        multi_select=True,
        show_multi_select_hint=True,
        multi_select_cursor_style=("fg_yellow", "bold"),
        multi_select_select_on_accept=False,
        title="Select Integration Tests to Run (↑/↓: move, SPACE: select, ENTER: confirm):",
    )

    selected_indices = terminal_menu.show()

    if selected_indices is None:
        # User cancelled (Ctrl+C or ESC)
        return None

    return [test_files[i] for i in selected_indices]


def fallback_select_tests(test_files: List[Path]) -> Optional[List[Path]]:
    """Fallback numbered selection for non-interactive terminals"""
    print("\nAvailable Integration Tests:")
    print("═" * 60)

    for i, test_file in enumerate(test_files, 1):
        print(f"{i:2d}. {test_file.name}")

    print("═" * 60)
    print("\nSelect tests:")
    print("  • Enter numbers (e.g., 1,3,5)")
    print("  • Enter ranges (e.g., 1-4)")
    print("  • Enter 'all' for all tests")
    print("  • Press Ctrl+C to cancel")

    while True:
        try:
            selection = input("\nYour selection: ").strip().lower()

            if selection == "all":
                return test_files

            # Parse selection
            selected_indices = set()

            for part in selection.split(","):
                part = part.strip()

                if "-" in part:
                    # Range
                    start, end = part.split("-", 1)
                    start_idx = int(start.strip())
                    end_idx = int(end.strip())

                    if not (1 <= start_idx <= len(test_files)) or not (
                        1 <= end_idx <= len(test_files)
                    ):
                        print(
                            f"Invalid range: {part}. Numbers must be between 1 and {len(test_files)}"
                        )
                        continue

                    selected_indices.update(range(start_idx, end_idx + 1))
                else:
                    # Single number
                    idx = int(part)
                    if not (1 <= idx <= len(test_files)):
                        print(
                            f"Invalid number: {idx}. Must be between 1 and {len(test_files)}"
                        )
                        continue
                    selected_indices.add(idx)

            if not selected_indices:
                print("No tests selected. Please try again.")
                continue

            # Convert to 0-based indices
            return [test_files[i - 1] for i in sorted(selected_indices)]

        except ValueError as e:
            print(f"Invalid input: {e}. Please try again.")
        except KeyboardInterrupt:
            print("\nCancelled.")
            return None


def get_cycle_count() -> Optional[int]:
    """Prompt user for number of cycles"""
    while True:
        try:
            cycles_input = input("\nNumber of cycles to run (default: 1): ").strip()

            if not cycles_input:
                return 1

            cycles = int(cycles_input)

            if cycles < 1:
                print("Number of cycles must be at least 1.")
                continue

            return cycles

        except ValueError:
            print("Invalid input. Please enter a positive integer.")
        except KeyboardInterrupt:
            print("\nCancelled.")
            return None


def get_stop_on_fail() -> bool:
    """Prompt user for stop-on-failure preference"""
    while True:
        try:
            response = (
                input("\nStop on first failure? (y/n, default: n): ").strip().lower()
            )

            if not response or response == "n":
                return False

            if response == "y":
                return True

            print("Please enter 'y' or 'n'.")

        except KeyboardInterrupt:
            print("\nCancelled.")
            return False


def run_test_cycle(
    test_file: Path,
    cycle: int,
    deployment_path: Path,
    timeout: Optional[int] = None,
) -> TestResult:
    """Execute single test file once via pytest subprocess"""
    cmd = [
        "pytest",
        str(test_file),
        "--deployment",
        str(deployment_path),
        "-v",
        "--tb=short",
        "--color=yes",
    ]

    start_time = datetime.now()

    try:
        result = subprocess.run(
            cmd,
            timeout=timeout,
            capture_output=False,  # Show output in real-time
            text=True,
        )

        duration = (datetime.now() - start_time).total_seconds()

        return TestResult(
            test_file=test_file.name,
            cycle=cycle,
            passed=(result.returncode == 0),
            duration=duration,
            exit_code=result.returncode,
            timestamp=start_time,
        )

    except subprocess.TimeoutExpired:
        duration = (datetime.now() - start_time).total_seconds()
        print(f"\n⚠️  Test timed out after {timeout}s")

        return TestResult(
            test_file=test_file.name,
            cycle=cycle,
            passed=False,
            duration=duration,
            exit_code=-1,
            timestamp=start_time,
        )

    except Exception as e:
        duration = (datetime.now() - start_time).total_seconds()
        print(f"\n❌ Test execution failed: {e}")

        return TestResult(
            test_file=test_file.name,
            cycle=cycle,
            passed=False,
            duration=duration,
            exit_code=-1,
            timestamp=start_time,
        )


def format_duration(seconds: float) -> str:
    """Format duration in human-readable form"""
    if seconds < 60:
        return f"{seconds:.1f}s"
    elif seconds < 3600:
        minutes = int(seconds // 60)
        secs = int(seconds % 60)
        return f"{minutes}m {secs}s"
    else:
        hours = int(seconds // 3600)
        minutes = int((seconds % 3600) // 60)
        return f"{hours}h {minutes}m"


def print_progress_bar(current: int, total: int, width: int = 40) -> str:
    """Generate a progress bar string"""
    filled = int(width * current / total)
    bar = "━" * filled + "━" * (width - filled)
    percentage = int(100 * current / total)
    return f"{bar} {percentage}%"


def print_final_summary(tracker: ResultsTracker, num_tests: int, num_cycles: int):
    """Print final execution summary"""
    summaries = tracker.get_summary()
    total_duration = tracker.get_total_duration()

    print("\n")
    print("═" * 70)
    print(" " * 20 + "TEST EXECUTION SUMMARY")
    print("═" * 70)

    total_executions = num_tests * num_cycles
    total_passed = sum(r.passed for r in tracker.results)
    overall_pass_rate = (
        (total_passed / len(tracker.results) * 100) if tracker.results else 0
    )

    print(
        f"Tests Run: {num_tests} files × {num_cycles} cycles = {total_executions} total executions"
    )
    print(f"Duration: {format_duration(total_duration.total_seconds())}")
    print()

    # Print per-test summaries
    for test_name in sorted(summaries.keys()):
        summary = summaries[test_name]

        print("┌" + "─" * 68 + "┐")
        print(f"│ {test_name:<66} │")

        # Pass rate with status indicator
        status = "⚠️  FLAKY" if summary.is_flaky else "✓ STABLE"
        if summary.pass_rate == 0:
            status = "✗ FAILED"

        pass_pct = summary.pass_rate * 100
        print(
            f"│   ✓ Passed: {summary.passed}/{summary.total_runs} ({pass_pct:.1f}%)  {status:<20} │"
        )

        # Average duration
        print(f"│   ⏱  Avg Duration: {format_duration(summary.avg_duration):<42} │")

        # Failed cycles (if any)
        if summary.failed_cycles:
            failed_str = ", ".join(str(c) for c in summary.failed_cycles)
            print(
                f"│   ✗ Failed cycles: [{failed_str}]{' ' * (44 - len(failed_str))} │"
            )

        print("└" + "─" * 68 + "┘")
        print()

    # Overall statistics
    print(
        f"Overall: {total_passed}/{len(tracker.results)} passed ({overall_pass_rate:.1f}%)"
    )
    print()

    # Flaky test warning
    flaky_tests = tracker.detect_flaky_tests()
    if flaky_tests:
        print("⚠️  Flaky Tests Detected:")
        for test_name in sorted(flaky_tests):
            summary = summaries[test_name]
            print(f"  • {test_name} ({summary.pass_rate * 100:.1f}% pass rate)")
        print()
        print("💡 Tip: Run flaky tests with more cycles for statistical significance")
    elif total_passed == len(tracker.results):
        print("✓ All tests passed consistently!")
    elif total_passed == 0:
        print("✗ All tests failed - check hardware connection and GDS status")

    print()


def run_tests(
    test_files: List[Path],
    num_cycles: int,
    deployment_path: Path,
    stop_on_fail: bool = False,
    timeout: Optional[int] = None,
) -> ResultsTracker:
    """Execute selected tests for specified number of cycles"""
    tracker = ResultsTracker()
    total_executions = len(test_files) * num_cycles
    current_execution = 0

    print("\n" + "═" * 70)
    print(f"Running {len(test_files)} tests × {num_cycles} cycles")
    print("═" * 70)
    print()

    try:
        for test_file in test_files:
            for cycle in range(1, num_cycles + 1):
                current_execution += 1

                # Progress header
                progress = print_progress_bar(current_execution, total_executions)
                print(f"\n{'─' * 70}")
                print(
                    f"Test: {test_file.name} | Cycle {cycle}/{num_cycles} | {progress}"
                )
                print(f"{'─' * 70}\n")

                # Run test
                result = run_test_cycle(test_file, cycle, deployment_path, timeout)
                tracker.add(result)

                # Result indicator
                status = "✓ PASSED" if result.passed else "✗ FAILED"
                print(
                    f"\n{status} - {test_file.name} (cycle {cycle}) - {format_duration(result.duration)}"
                )

                # Stop on failure if requested
                if not result.passed and stop_on_fail:
                    print("\n⚠️  Stopping due to failure (--stop-on-fail)")
                    return tracker

    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted by user (Ctrl+C)")
        print("Showing partial results collected so far...\n")

    return tracker


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description="Interactive integration test runner for PROVES CubeSat",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Interactive mode (cursor navigation)
  %(prog)s

  # Run specific tests with CLI arguments
  %(prog)s --tests watchdog_test imu_manager_test --cycles 5

  # Run all tests
  %(prog)s --all --cycles 10

  # Stop on first failure
  %(prog)s --tests watchdog_test --cycles 20 --stop-on-fail
        """,
    )

    parser.add_argument(
        "--tests",
        nargs="+",
        metavar="TEST",
        help="Specific test files to run (with or without .py extension)",
    )

    parser.add_argument(
        "--all",
        action="store_true",
        help="Run all discovered tests",
    )

    parser.add_argument(
        "--cycles",
        type=int,
        default=1,
        metavar="N",
        help="Number of cycles to run each test (default: 1)",
    )

    parser.add_argument(
        "--deployment",
        type=Path,
        metavar="PATH",
        help="Override deployment path (default: build-artifacts/zephyr/fprime-zephyr-deployment)",
    )

    parser.add_argument(
        "--stop-on-fail",
        action="store_true",
        help="Stop execution on first test failure",
    )

    parser.add_argument(
        "--continue-on-fail",
        action="store_true",
        help="Continue execution despite failures (default behavior)",
    )

    parser.add_argument(
        "--timeout",
        type=int,
        metavar="SECONDS",
        help="Timeout per test execution in seconds (optional)",
    )

    args = parser.parse_args()

    # Determine project root
    script_path = Path(__file__).resolve()
    project_root = script_path.parent.parent.parent

    # Determine deployment path
    if args.deployment:
        deployment_path = args.deployment.resolve()
    else:
        deployment_path = (
            project_root / "build-artifacts" / "zephyr" / "fprime-zephyr-deployment"
        )

    if not deployment_path.exists():
        print(f"❌ Error: Deployment directory not found: {deployment_path}")
        print("\nMake sure you've built the project first:")
        print("  make build")
        sys.exit(1)

    # Discover tests
    test_dir = project_root / "FprimeZephyrReference" / "test" / "int"

    if not test_dir.exists():
        print(f"❌ Error: Test directory not found: {test_dir}")
        sys.exit(1)

    all_tests = discover_tests(test_dir)

    if not all_tests:
        print(f"❌ Error: No integration tests found in {test_dir}")
        sys.exit(1)

    print(f"Found {len(all_tests)} integration tests")

    # Determine which tests to run
    selected_tests: Optional[List[Path]] = None

    if args.tests:
        # CLI mode: specific tests
        selected_tests = []
        for test_name in args.tests:
            # Add .py extension if not present
            if not test_name.endswith(".py"):
                test_name = f"{test_name}.py"

            # Find matching test
            matching = [t for t in all_tests if t.name == test_name]

            if not matching:
                print(f"❌ Error: Test '{test_name}' not found")
                print("\nAvailable tests:")
                for t in all_tests:
                    print(f"  • {t.name}")
                sys.exit(1)

            selected_tests.append(matching[0])

    elif args.all:
        # CLI mode: all tests
        selected_tests = all_tests

    else:
        # Interactive mode
        print("\n" + "═" * 70)
        print("PROVES CubeSat - Interactive Integration Test Runner")
        print("═" * 70)

        if not HAS_INTERACTIVE:
            print(
                "\n⚠️  Note: simple-term-menu not installed - using fallback numbered selection"
            )
            print("Install for better UX: ./bin/uv pip install simple-term-menu\n")

        selected_tests = interactive_select_tests(all_tests)

        if selected_tests is None or len(selected_tests) == 0:
            print("No tests selected. Exiting.")
            sys.exit(0)

        print(f"\n✓ Selected {len(selected_tests)} tests")

        # Get cycle count
        num_cycles = get_cycle_count()
        if num_cycles is None:
            sys.exit(0)

        args.cycles = num_cycles

        # Get stop-on-fail preference
        args.stop_on_fail = get_stop_on_fail()

    # Run tests
    tracker = run_tests(
        test_files=selected_tests,
        num_cycles=args.cycles,
        deployment_path=deployment_path,
        stop_on_fail=args.stop_on_fail,
        timeout=args.timeout,
    )

    # Print summary
    print_final_summary(tracker, len(selected_tests), args.cycles)

    # Exit code: 0 if all passed, 1 if any failed
    all_passed = all(r.passed for r in tracker.results)
    sys.exit(0 if all_passed else 1)


if __name__ == "__main__":
    main()
