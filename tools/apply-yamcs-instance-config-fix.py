"""Apply PROVES-specific fixes to the fprime-yamcs instance config template.

Replaces placeholder preprocessor/postprocessor class names with the correct
YAMCS built-in CCSDS classes, corrects the frame/packet length from the
generic 1024 default to 248 bytes, and removes the ProcessRunner service
(we launch fprime-yamcs-events separately with --dictionary).
"""

import sys

if len(sys.argv) != 2:
    print(
        f"Usage: {sys.argv[0]} <path-to-instance-config>",
        file=sys.stderr,
    )
    sys.exit(1)
path = sys.argv[1]
content = open(path).read()

if (
    "frameLength: 248" in content
    and "CfsPacketPreprocessor" in content
    and "rootContainer" in content
):
    print("⚠ fprime-yamcs instance config fix already applied")
    sys.exit(0)

fixes = [
    # Replace placeholder preprocessor with CfsPacketPreprocessor + useLocalGenerationTime.
    # YAMCS 5.12 requires packetPreprocessorClassName (mandatory field in VcDownlinkManagedParameters).
    # CcsdsPacketPreprocessor is abstract; GenericPacketPreprocessor requires timestampOffset/seqCountOffset.
    # CfsPacketPreprocessor handles standard CCSDS primary headers and, with useLocalGenerationTime: true,
    # uses reception time instead of reading a CFS secondary header — correct for F Prime packets.
    (
        "        packetPreprocessorClassName: com.example.myproject.MyPacketPreprocessor\n",
        "        packetPreprocessorClassName: org.yamcs.tctm.cfs.CfsPacketPreprocessor\n"
        "        packetPreprocessorArgs:\n"
        "          useLocalGenerationTime: true\n",
    ),
    # Remove placeholder postprocessor class (not required for TC)
    (
        "        commandPostprocessorClassName: com.example.myproject.MyCommandPostprocessor\n",
        "",
    ),
    # Correct frame length (template default 1024 is wrong for PROVES: 248 bytes)
    (
        "    frameLength: 1024\n",
        "    frameLength: 248        # ComCfg.fpp: TmFrameFixedSize = 248\n",
    ),
    ("    maxPacketLength: 1024\n", "    maxPacketLength: 248\n"),
    ("    maxFrameLength: 1024\n", "    maxFrameLength: 248\n"),
    # Remove ProcessRunner for fprime-yamcs-events (we start it separately with --dictionary)
    (
        "  - class: org.yamcs.ProcessRunner\n"
        "    args:\n"
        '      command: ["fprime-yamcs-events"]\n'
        '      restart: "always"\n'
        "      logLevel: INFO\n",
        "",
    ),
    # Add rootContainer to tm_realtime stream so XtceTmExtractor knows where to start.
    # Without this, Mdb.getRootSequenceContainer() returns null (fprime-to-xtce doesn't
    # set a root in the XTCE), and StreamTmPacketProvider logs a warning and does no
    # container matching — packets arrive but zero parameters are extracted.
    (
        '    - name: "tm_realtime"\n'
        '      processor: "realtime"\n'
        '    - name: "tm_dump"\n',
        '    - name: "tm_realtime"\n'
        '      processor: "realtime"\n'
        '      rootContainer: "/ReferenceDeployment|ReferenceDeployment/CCSDSSpacePacket"\n'
        '    - name: "tm_dump"\n',
    ),
]

fixed = content
for old, new in fixes:
    fixed = fixed.replace(old, new)

if fixed == content:
    print("❌ Error: no changes made — template may have changed upstream")
    sys.exit(1)

open(path, "w").write(fixed)
print("✓ Applied fprime-yamcs instance config fix")
