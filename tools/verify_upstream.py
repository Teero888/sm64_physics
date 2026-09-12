#!/usr/bin/env python3
"""Verify the complete vendored file set against its pinned import manifest."""
import hashlib
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
manifest = json.loads((root / "upstream.json").read_text())
actual = {p.relative_to(root / "upstream").as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
          for p in (root / "upstream").rglob("*") if p.is_file()}
if actual != manifest["files"]:
    for name in sorted(actual.keys() | manifest["files"].keys()):
        if actual.get(name) != manifest["files"].get(name):
            print(f"Missing, modified, or unrecorded upstream file: {name}", file=sys.stderr)
    sys.exit(1)
print(f"Verified {len(actual)} unchanged upstream files at {manifest['revision']}")
