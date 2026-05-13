"""Apply the fprime-yamcs-events busy-wait CPU fix to the installed package."""

import re
import sys

path = sys.argv[1]
content = open(path).read()

if "subscription.result()" in content:
    print("⚠ fprime-yamcs-events CPU fix already applied")
    sys.exit(0)

fixed = re.sub(
    r"# Keep the script running\s*\n([ \t]*)while True:\s*\n\1[ \t]+pass",
    r"# Block until the WebSocket subscription ends (no CPU spin)\n\1subscription.result()",
    content,
)

if fixed == content:
    print(
        "❌ Error: pattern not found in events processor — fix may not be needed or file changed"
    )
    sys.exit(1)

open(path, "w").write(fixed)
print("✓ Applied fprime-yamcs-events CPU fix")
