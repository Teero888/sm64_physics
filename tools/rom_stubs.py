#!/usr/bin/env python3
"""Stand-ins for the textures the game's code includes from the ROM.

  rom_stubs.py ROM_ASSETS_TSV OUT_DIR

The decomp's model and level files include each texture's pixels as a
generated .inc.c. The library does not carry them: each becomes a texture of
the same size whose first bytes say where its pixels are in the ROM (the tag
platform/rom.h reads) and whose rest is zero. The textures keep their names,
sizes and distinct addresses, the display lists still point at them, and
drawing finds the pixels in the user's ROM (sm64_texture).
"""
import json
import sys
from pathlib import Path


def tag(where):
    """The tag's twelve bytes for a ROM location: MIO0 block and offset in it,
    or a file offset (block 0xFFFFFFFF). Little-endian words after "sm64"."""
    block, offset = (where[0], where[1]) if len(where) == 2 else (0xFFFFFFFF, where[0])
    data = b"sm64" + block.to_bytes(4, "little") + offset.to_bytes(4, "little")
    return ",".join(f"0x{b:02x}" for b in data)


def stub(where, size):
    return f"{tag(where)}, [{size - 1}] = 0,\n"


def main():
    table, out = Path(sys.argv[1]), Path(sys.argv[2])
    for line in table.read_text().splitlines():
        if line.startswith("#"):
            continue
        kind, path, size, where = line.split("\t")[:4]
        if kind != "texture":
            continue
        path = out / path
        text = stub(json.loads(where), int(size))
        if not path.exists() or path.read_text() != text:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)


if __name__ == "__main__":
    main()
