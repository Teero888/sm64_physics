#!/usr/bin/env python3
"""Copy selected complete functions verbatim out of mixed upstream files.

No replacement bodies, regex substitutions, or source patches are applied.
The manifest records both the original file and each selected byte range.
"""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
EXTRACTIONS = {
    "animation.c": {
        "source": "src/engine/graph_node.c",
        "headers": ["sm64.h", "engine/graph_node.h", "game/area.h", "game/memory.h"],
        "functions": ["geo_obj_init_animation", "geo_obj_init_animation_accel",
                      "retrieve_animation_index", "geo_update_animation_frame"],
    },
}

def function_range(data, name):
    # Blank comments and literals without changing offsets. Braces in comments
    # and strings must not terminate a function body.
    token = rb'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    masked = re.sub(token, lambda m: bytes(10 if b == 10 else 32 for b in m[0]),
                    data, flags=re.S)
    pattern = rb'^[^\n;{}]*\b' + name.encode() + rb'\s*\([^;{}]*\)\s*\{'
    matches = list(re.finditer(pattern, masked, re.M))
    if len(matches) != 1:
        raise ValueError(f"Expected one definition of {name}, found {len(matches)}")
    start = matches[0].start()
    depth = 1
    end = matches[0].end()
    while depth and end < len(masked):
        depth += (masked[end] == ord('{')) - (masked[end] == ord('}'))
        end += 1
    if depth:
        raise ValueError(f"Unterminated function {name}")
    return start, end

def main():
    checkout = Path(sys.argv[1]).resolve()
    upstream = json.loads((ROOT / "upstream.json").read_text())
    revision = subprocess.check_output(
        ["git", "-C", str(checkout), "rev-parse", "HEAD"], text=True).strip()
    if revision != upstream["revision"]:
        raise ValueError("Checkout does not match the pinned upstream revision")
    outputs = {}
    for output, spec in EXTRACTIONS.items():
        # Read committed bytes, not potentially edited working-tree files.
        data = subprocess.check_output(
            ["git", "-C", str(checkout), "show", f"{revision}:{spec['source']}"])
        contents = b"/* Generated verbatim upstream function extraction. See extracted.json. */\n"
        for header in spec["headers"]:
            if not (ROOT / "upstream" / "include" / header).exists() and not (
                    ROOT / "upstream" / "src" / header).exists():
                raise ValueError(f"Missing imported header {header}")
            contents += f'#include "{header}"\n'.encode()
        functions = {}
        for name in spec["functions"]:
            start, end = function_range(data, name)
            body = data[start:end]
            line = data[:start].count(b"\n") + 1
            contents += f'\n#line {line} "n64decomp/{spec["source"]}"\n'.encode() + body + b"\n"
            functions[name] = {"start": start, "end": end,
                               "sha256": hashlib.sha256(body).hexdigest()}
        (ROOT / "extracted").mkdir(exist_ok=True)
        (ROOT / "extracted" / output).write_bytes(contents)
        outputs[output] = {"source": spec["source"],
                           "source_sha256": hashlib.sha256(data).hexdigest(),
                           "sha256": hashlib.sha256(contents).hexdigest(),
                           "functions": functions}
    (ROOT / "extracted.json").write_text(json.dumps({"revision": revision, "files": outputs}, indent=2) + "\n")

if __name__ == "__main__":
    main()
