#!/usr/bin/env python3
"""Stand-ins for the textures the game's code includes from the ROM.

  rom_stubs.py ROM_ASSETS_TSV OUT_DIR

The decomp's model and level files include each texture's pixels as a
generated .inc.c. The library does not carry them: each becomes one zero, so
the textures keep their names (and distinct addresses) and the display lists
still point at them. Drawing gets the pixels from the user's ROM.
"""
import sys
from pathlib import Path


def main():
    table, out = Path(sys.argv[1]), Path(sys.argv[2])
    for line in table.read_text().splitlines():
        if line.startswith("#"):
            continue
        kind, path = line.split("\t")[:2]
        if kind != "texture":
            continue
        stub = out / path
        if not stub.exists():
            stub.parent.mkdir(parents=True, exist_ok=True)
            stub.write_text("0,\n")


if __name__ == "__main__":
    main()
