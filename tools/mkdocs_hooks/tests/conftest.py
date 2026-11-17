"""Make the hook module importable as `component_docs`, the way MkDocs loads it."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
