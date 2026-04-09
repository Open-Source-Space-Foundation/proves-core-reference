"""Patch FPrimeEventProcessor to include event args in the send_event extra field.

The extra field is a free-form str→str mapping that YAMCS stores alongside each
event.  By including event arguments there, the opcode_ack_bridge can recover the
opcode value from OpCodeCompleted events without needing to re-decode the raw
CCSDS packet or parse the human-readable message string.
"""

import sys

if len(sys.argv) != 2:
    print(
        f"Usage: {sys.argv[0]} <path-to-processor.py>",
        file=sys.stderr,
    )
    sys.exit(1)

path = sys.argv[1]
content = open(path).read()

SENTINEL = "extra=event_args if event_args else None,"

if SENTINEL in content:
    print("⚠ fprime-yamcs-events opcode-ack fix already applied")
    sys.exit(0)

OLD = (
    "            self.yamcs_client.send_event(\n"
    "                instance=self.yamcs_instance,\n"
    "                source='FPrimeEventProcessor',\n"
    "                event_type=event_name,\n"
    "                severity=yamcs_severity,\n"
    "                message=message,\n"
    "            )\n"
)
NEW = (
    "            self.yamcs_client.send_event(\n"
    "                instance=self.yamcs_instance,\n"
    "                source='FPrimeEventProcessor',\n"
    "                event_type=event_name,\n"
    "                severity=yamcs_severity,\n"
    "                message=message,\n"
    "                extra=event_args if event_args else None,\n"
    "            )\n"
)

fixed = content.replace(OLD, NEW)

if fixed == content:
    print(
        "❌ Error: pattern not found in events processor — fix may not apply or upstream changed"
    )
    sys.exit(1)

open(path, "w").write(fixed)
print("✓ Applied fprime-yamcs-events opcode-ack fix")
