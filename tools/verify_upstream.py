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
    imported = root / "upstream" / entry["source"]
    if imported.exists():
        original = imported.read_bytes()
    if len(sys.argv) == 2:
        original = subprocess.check_output([
            "git", "-C", sys.argv[1], "show", f"{manifest['revision']}:{entry['source']}"])
    if original is not None and hashlib.sha256(original).hexdigest() != entry["source_sha256"]:
        sys.exit(f"Original source mismatch: {entry['source']}")
    if "copied_ranges" in entry:
        if original is None:
            sys.exit(f"Missing original for source-range extraction: {name}")
        excluded = sorted(function_range(original, function)
                          for function in entry["omitted_functions"])
        expected_ranges = []
        previous = 0
        for start, end in excluded + [(len(original), len(original))]:
            expected_ranges.append((previous, start))
            previous = end
        if [(part["start"], part["end"]) for part in entry["copied_ranges"]] != expected_ranges:
            sys.exit(f"Extraction drops bytes outside the named omitted functions: {name}")
        rebuilt = b"/* Generated verbatim upstream function extraction. See extracted.json. */\n"
        rebuilt += f'#line 1 "n64decomp/{entry["source"]}"\n'.encode()
        previous = 0
        for part in entry["copied_ranges"]:
            start, end = part["start"], part["end"]
            body = original[start:end]
            if hashlib.sha256(body).hexdigest() != part["sha256"]:
                sys.exit(f"Source range mismatch: {name}")
            rebuilt += b"\n" * original[previous:start].count(b"\n") + body
            previous = end
        if rebuilt != data:
            sys.exit(f"Extraction differs from original ranges: {name}")
    for function, expected in entry["functions"].items():
        start, end = function_range(data, function)
        body = data[start:end]
        if hashlib.sha256(body).hexdigest() != expected["sha256"]:
            sys.exit(f"Modified function: {function}")
        if original is not None and body != original[expected["start"]:expected["end"]]:
            sys.exit(f"Function differs from the upstream commit: {function}")
print(f"Verified {len(extracted['files'])} verbatim function extractions")
