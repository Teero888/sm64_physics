#!/usr/bin/env python3
"""Movies made up for a version without a TAS to check it with.

  make_movie.py idle OUT.m64 POLLS [VI_RATE] [--rom ROM]
  make_movie.py random OUT.m64 POLLS SEED [VI_RATE] [--rom ROM]

idle presses nothing: the title screen plays its demos, over and over.
random holds random buttons and stick for 8 polls at a time, from power-on:
it finds its way through the menus into the game and plays.
Both are .m64 from power-on, for sm64_oracle and sm64_lockstep with
--poll-offset 0 (EU: VI_RATE 50). With --rom, the header names the ROM (its
checksum and country code), as movies recorded on an emulator do, so FrameTee
knows which version's movie it is.
"""
import math
import random
import struct
import sys


def header(polls, vi_rate, rom=None):
    h = bytearray(0x400)
    h[0:4] = b"M64\x1a"
    struct.pack_into("<I", h, 4, 3)
    h[0x14] = vi_rate
    h[0x15] = 1
    struct.pack_into("<I", h, 0x18, polls)
    struct.pack_into("<H", h, 0x1C, 2)  # from power-on
    h[0xC4:0xC4 + 14] = b"SUPER MARIO 64"
    if rom:
        # A .z64's header: the first checksum, stored in the ROM's byte order,
        # and the country code.
        h[0xE4:0xE8] = rom[0x10:0x14]
        h[0xE8] = rom[0x3E]
    return h


def random_inputs(polls, seed):
    rng = random.Random(seed)
    out = bytearray()
    held, stick = 0, (0, 0)
    for i in range(polls):
        if i % 8 == 0:
            held = 0
            for bit, chance in ((0x80, 0.35), (0x40, 0.15), (0x20, 0.1), (0x10, 0.03), (0x2000, 0.05),
                                (0x1000, 0.05), (0x800, 0.05), (0x400, 0.05)):
                if rng.random() < chance:
                    held |= bit
            angle, radius = rng.random() * 6.283, rng.choice((0, 40, 80, 127))
            stick = (int(radius * math.cos(angle)), int(radius * math.sin(angle)))
        out += struct.pack("<Hbb", held, max(-128, min(127, stick[0])), max(-128, min(127, stick[1])))
    return out


def main():
    rom = None
    if "--rom" in sys.argv:
        at = sys.argv.index("--rom")
        with open(sys.argv[at + 1], "rb") as f:
            rom = f.read(0x40)
        if rom[:4] != b"\x80\x37\x12\x40":
            sys.exit("--rom needs a .z64 (big-endian) ROM")
        del sys.argv[at:at + 2]
    if len(sys.argv) < 4 or sys.argv[1] not in ("idle", "random"):
        sys.exit(__doc__)
    kind, path, polls = sys.argv[1], sys.argv[2], int(sys.argv[3])
    if kind == "idle":
        vi_rate = int(sys.argv[4]) if len(sys.argv) > 4 else 60
        body = bytes(4 * polls)
    else:
        vi_rate = int(sys.argv[5]) if len(sys.argv) > 5 else 60
        body = random_inputs(polls, int(sys.argv[4]))
    with open(path, "wb") as f:
        f.write(bytes(header(polls, vi_rate, rom)) + bytes(body))


if __name__ == "__main__":
    main()
