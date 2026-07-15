"""Make the Zephyr SDK's macOS toolchain download fall back to curl if the bundled wget fails.

Some macOS hosts (observed on a self-hosted CI runner) fail every invocation
of the SDK's bundled hosttools wget when downloading a GNU toolchain archive,
even against a freshly extracted SDK, while a plain curl to the same URL
succeeds. This appends a curl fallback to the wget invocation in setup.sh so
a wget failure doesn't abort the install.
"""

import sys

path = sys.argv[1]
content = open(path).read()

marker = 'curl -fSL --retry 3 -o "${toolchain_filename}" "${toolchain_uri}"'
if marker in content:
    print("⚠ Zephyr SDK curl fallback already applied")
    sys.exit(0)

original = '${wget} -q --show-progress -N -O "${toolchain_filename}" "${toolchain_uri}"'
patched = f"{original} || {marker}"

if original not in content:
    print(
        "❌ Error: wget download line not found in setup.sh — SDK setup.sh may have changed"
    )
    sys.exit(1)

open(path, "w").write(content.replace(original, patched))
print("✓ Applied Zephyr SDK curl fallback")
