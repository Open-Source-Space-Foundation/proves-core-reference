"""Inspect a built prototype site: assets, image src, edit links, Mermaid blocks.

Usage: python3 check_site.py <site_dir>
"""

import re
import sys
from pathlib import Path

site = Path(sys.argv[1])
comp = site / "components"


def html(rel):
    return (site / rel).read_text()


print("asset files:", sorted(str(p.relative_to(site)) for p in comp.rglob("*.svg")))
print("Watchdog img src:", re.findall(r'<img[^>]*src="([^"]*)"', html("components/Watchdog/sdd/index.html")))
for page in [
    "components/Watchdog/sdd/index.html",
    "components/ModeManager/safe-mode-fsm/index.html",
    "getting-started/initial-setup/index.html",
]:
    edits = re.findall(r'href="([^"]*)"[^>]*title="Edit this page"', html(page))
    print(f"edit link {page}: {edits}")
print("safe-mode-fsm -> sdd link:", re.findall(r'href="(\.\./sdd/)"', html("components/ModeManager/safe-mode-fsm/index.html")))
pages = sorted(comp.rglob("index.html"))
mermaid_pages = [p for p in pages if '<pre class="mermaid">' in p.read_text()]
print(f"component pages: {len(pages)}; pages with <pre class=\"mermaid\">: {len(mermaid_pages)}")
print("PayloadCom pages:", sorted(str(p.parent.relative_to(site)) for p in (comp / "PayloadCom").rglob("index.html")))
