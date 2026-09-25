#!/usr/bin/env python3
"""The pointers among the state's initial values.

  init_pointers.py OBJECT OUT_C

OBJECT is the library's object (sm64_game.o, platform/state.ld). Its
relocations in sm64_state say which initialized words hold an address; those
whose target is itself in sm64_state point into the state and have to move
with every copy of it (platform/world.c). Writes their offsets as a C table.

Fails if data outside the state holds an address in it: code that reads such
a pointer would reach the section itself, not the current world's copy.

Reads ELF objects (with readelf) and, for Windows builds, COFF ones.
"""
import re
import struct
import subprocess
import sys


def coff(data):
    """(offsets, outside) of an x86-64 COFF object: the relocations as above.
    MinGW's relocatable link writes it behind a DOS and PE header."""
    base = struct.unpack_from("<I", data, 0x3c)[0] + 4 if data[:2] == b"MZ" else 0
    sections = struct.unpack_from("<H", data, base + 2)[0]
    symbol_table, symbol_count = struct.unpack_from("<II", data, base + 8)
    strings = symbol_table + 18 * symbol_count

    def name(raw):
        if raw[:4] == b"\0\0\0\0":
            offset = struct.unpack_from("<I", raw, 4)[0]
            return data[strings + offset:data.index(b"\0", strings + offset)].decode("latin-1")
        return raw.rstrip(b"\0").decode("latin-1")

    headers = []
    for i in range(sections):
        h = base + 20 + struct.unpack_from("<H", data, base + 16)[0] + 40 * i
        raw = data[h:h + 8]
        if raw[:1] == b"/":
            offset = int(raw[1:].rstrip(b"\0"))
            section_name = data[strings + offset:data.index(b"\0", strings + offset)].decode("latin-1")
        else:
            section_name = raw.rstrip(b"\0").decode("latin-1")
        relocations, count = struct.unpack_from("<I", data, h + 24)[0], struct.unpack_from("<H", data, h + 32)[0]
        if struct.unpack_from("<I", data, h + 36)[0] & 0x01000000:  # more than 65535: the first holds the count
            count = struct.unpack_from("<I", data, relocations)[0]
            relocations, count = relocations + 10, count - 1
        headers.append((section_name, relocations, count))
    state = next(i + 1 for i, (n, _, _) in enumerate(headers) if n == "sm64_state")
    symbols, i = [], 0
    while i < symbol_count:
        at = symbol_table + 18 * i
        section = struct.unpack_from("<h", data, at + 12)[0]
        symbols.append((name(data[at:at + 8]), section))
        aux = data[at + 17]
        symbols.extend([("", 0)] * aux)
        i += 1 + aux
    offsets, outside = [], []
    for section_name, relocations, count in headers:
        for r in range(count):
            address, index, kind = struct.unpack_from("<IIH", data, relocations + 10 * r)
            target_name, target_section = symbols[index]
            if target_section != state:
                continue
            if section_name != "sm64_state":
                # .refptr: MinGW's indirection for addresses of data in other
                # objects, which code loads and WORLD() moves.
                if not section_name.startswith((".text", ".pdata", ".xdata", ".debug", "sm64_state_variables",
                                                ".rdata$.refptr.")):
                    outside.append(f"{section_name}+0x{address:x} -> {target_name}")
                continue
            if kind != 1:  # IMAGE_REL_AMD64_ADDR64
                sys.exit(f"init_pointers.py: unexpected relocation type {kind} in sm64_state")
            offsets.append(address)
    return sorted(offsets), outside


def elf(obj):
    """(offsets, outside) of an ELF object."""
    sections = subprocess.run(["readelf", "-SW", obj], check=True, capture_output=True, text=True).stdout
    names = {int(m.group(1)): m.group(2) for m in re.finditer(r"^\s*\[\s*(\d+)\]\s+(\S+)", sections, re.M)}
    state = next(i for i, n in names.items() if n == "sm64_state")
    symbols = subprocess.run(["readelf", "-sW", obj], check=True, capture_output=True, text=True).stdout
    in_state = set()
    for line in symbols.splitlines():
        parts = line.split()
        if len(parts) == 8 and parts[6] == str(state):
            in_state.add(parts[7])
    relocations = subprocess.run(["readelf", "-rW", obj], check=True, capture_output=True, text=True).stdout
    offsets, current, outside = [], None, []
    for line in relocations.splitlines():
        if line.startswith("Relocation section"):
            current = re.search(r"'\.rela(\S+)'", line).group(1)
            continue
        parts = line.split()
        if current is None or len(parts) < 5 or not re.fullmatch(r"[0-9a-f]+", parts[0]):
            continue
        if current != "sm64_state":
            # Code reaches the state through WORLD(); data must not.
            if not current.startswith((".text", ".eh_frame", ".debug", "sm64_state_variables")) and (
                    parts[4] == "sm64_state" or parts[4] in in_state):
                outside.append(f"{current}+0x{parts[0]} -> {parts[4]}")
            continue
        if parts[2] != "R_X86_64_64":
            sys.exit(f"init_pointers.py: unexpected relocation {parts[2]} in sm64_state")
        target = parts[4]
        if target == "sm64_state" or target in in_state:
            offsets.append(int(parts[0], 16))
    return offsets, outside


def main():
    obj, out = sys.argv[1], sys.argv[2]
    with open(obj, "rb") as f:
        data = f.read()
    offsets, outside = elf(obj) if data[:4] == b"\x7fELF" else coff(data)
    if outside:
        sys.exit("init_pointers.py: data outside the state points into it:\n  " + "\n  ".join(outside))
    with open(out, "w") as f:
        f.write("// Generated by tools/state/init_pointers.py: the offsets in the state of\n"
                "// initial values that are addresses in the state.\n"
                "#include <stddef.h>\n#include <stdint.h>\n\n"
                f"const uint32_t gHostStatePointers[] = {{\n")
        for i in range(0, len(offsets), 8):
            f.write("    " + ", ".join(f"0x{o:x}" for o in offsets[i:i + 8]) + ",\n")
        f.write(f"}};\nconst size_t gHostStatePointerCount = {len(offsets)};\n")
    print(f"{len(offsets)} pointers in the state's initial values")


if __name__ == "__main__":
    main()
