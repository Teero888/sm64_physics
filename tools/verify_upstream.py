#!/usr/bin/env python3
"""Verify the complete vendored file set against its pinned import manifest."""
import hashlib
import json
from pathlib import Path
import sys
import subprocess
from extract_upstream import function_range

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

extracted = json.loads((root / "extracted.json").read_text())
if extracted["revision"] != manifest["revision"]:
    sys.exit("Extraction revision differs from the upstream import")
actual_extracted = {p.relative_to(root / "extracted").as_posix()
                    for p in (root / "extracted").rglob("*") if p.is_file()}
if actual_extracted != extracted["files"].keys():
    sys.exit("Missing or unrecorded extracted files")
for name, entry in extracted["files"].items():
    data = (root / "extracted" / name).read_bytes()
    if hashlib.sha256(data).hexdigest() != entry["sha256"]:
        sys.exit(f"Modified extraction: {name}")
    original = None
    if len(sys.argv) == 2:
        original = subprocess.check_output([
            "git", "-C", sys.argv[1], "show", f"{manifest['revision']}:{entry['source']}"])
        if hashlib.sha256(original).hexdigest() != entry["source_sha256"]:
            sys.exit(f"Original source mismatch: {entry['source']}")
    for function, expected in entry["functions"].items():
        start, end = function_range(data, function)
        body = data[start:end]
        if hashlib.sha256(body).hexdigest() != expected["sha256"]:
            sys.exit(f"Modified function: {function}")
        if original is not None and body != original[expected["start"]:expected["end"]]:
            sys.exit(f"Function differs from the upstream commit: {function}")
print(f"Verified {len(extracted['files'])} verbatim function extractions")
