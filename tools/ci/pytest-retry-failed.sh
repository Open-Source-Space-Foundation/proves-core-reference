#!/usr/bin/env bash
# Run a pytest command; if any tests fail, rerun only those tests once.
#
#   tools/ci/pytest-retry-failed.sh <pytest command...>
#
# Exits with the rerun's status, so the step passes when every failed test
# passes on its retry. Tests that passed only on retry are listed in the
# GitHub job summary and as warning annotations so each one can be ticketed.
set -uo pipefail

report_dir="$(mktemp -d)"
first_xml="${report_dir}/first.xml"
retry_xml="${report_dir}/retry.xml"

"$@" --junitxml="${first_xml}"
rc=$?
# 1 = tests ran and some failed. Anything else (pass, interrupted, internal
# error, usage error, nothing collected) is not something a rerun fixes.
if [ "${rc}" -ne 1 ]; then
  exit "${rc}"
fi

echo "::group::Rerunning failed tests once"
"$@" --last-failed --last-failed-no-failures none --junitxml="${retry_xml}"
rc=$?
echo "::endgroup::"

python3 - "${retry_xml}" "${rc}" <<'PY'
import os
import sys
import xml.etree.ElementTree as ET

retry_xml, rc = sys.argv[1], int(sys.argv[2])
passed, failed = [], []
if os.path.exists(retry_xml):
    for case in ET.parse(retry_xml).iter("testcase"):
        nodeid = f"{case.get('classname')}::{case.get('name')}"
        outcomes = {child.tag for child in case}
        if outcomes & {"failure", "error"}:
            failed.append(nodeid)
        elif "skipped" not in outcomes:
            passed.append(nodeid)

for nodeid in passed:
    print(f"::warning title=Passed on retry::{nodeid}")

job = os.environ.get("GITHUB_JOB", "pytest")
lines = [f"### {job}: rerun of failed tests\n"]
if passed:
    lines.append("**Passed on retry** (flaky, ticket each one):\n")
    lines += [f"- `{nodeid}`" for nodeid in passed]
    lines.append("")
if failed:
    lines.append("**Failed again on retry:**\n")
    lines += [f"- `{nodeid}`" for nodeid in failed]
    lines.append("")
if not passed and not failed:
    lines.append(f"Rerun reported no test results (pytest exit {rc}).\n")
report = "\n".join(lines) + "\n"

print(report)
summary = os.environ.get("GITHUB_STEP_SUMMARY")
if summary:
    with open(summary, "a") as f:
        f.write(report)
PY

exit "${rc}"
