#!/usr/bin/env python3
"""Apply patches/*.patch to copies of the decomp's files.

  overlay.py DECOMP_DIR PATCH_DIR OUT_DIR

Every file a patch touches is copied from DECOMP_DIR to OUT_DIR (same relative
path) and patched there, in patch-name order; the decomp checkout itself is
never modified. Prints the relative paths of the patched files, one per line,
for the build to compile them in place of the originals. OUT_DIR is rebuilt
from scratch each time, so a removed patch leaves nothing behind.
"""
import re
import shutil
import subprocess
import sys
from pathlib import Path


def touched(patch_text):
    files = []
    for line in patch_text.splitlines():
        match = re.match(r"^\+\+\+ b/(\S+)", line)
        if match and match.group(1) not in files:
            files.append(match.group(1))
    return files


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    decomp, patches, out = (Path(p) for p in sys.argv[1:])
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    patched = []
    for patch in sorted(patches.glob("*.patch")):
        text = patch.read_text()
        for relative in touched(text):
            target = out / relative
            if not target.exists():
                source = decomp / relative
                if not source.exists():
                    sys.exit(f"{patch.name}: {relative} is not in {decomp}")
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, target)
                patched.append(relative)
        result = subprocess.run(["patch", "-p1", "--no-backup-if-mismatch", "-f", "-s", "-d", str(out)],
                                input=text, text=True, capture_output=True)
        if result.returncode != 0:
            sys.exit(f"{patch.name} does not apply:\n{result.stdout}{result.stderr}")
    print("\n".join(patched))


if __name__ == "__main__":
    main()
