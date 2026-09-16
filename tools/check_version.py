#!/usr/bin/env python3
from pathlib import Path
import re

expected = "0.6.5"
checks = {
    "include/nexus/nexus_version.h": r'#define NEXUS_VERSION_STRING "([^"]+)"',
    "Makefile": r'^NEXUS_VERSION \?= (\S+)',
    "README.md": r'\*\*NexusOS ([0-9.]+) — Enstein\*\*',
    "docs/STATUS.md": r'^# NexusOS ([0-9.]+) — Enstein',
}
for name, pat in checks.items():
    text = Path(name).read_text()
    m = re.search(pat, text, re.M)
    if not m:
        raise SystemExit(f"missing version marker: {name}")
    if m.group(1) != expected:
        raise SystemExit(f"version mismatch: {name}: {m.group(1)} != {expected}")
print(f"NexusOS version consistency: {expected} [ OK ]")
