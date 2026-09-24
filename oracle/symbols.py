#!/usr/bin/env python3
"""Write the symbols.tsv the oracle reads (name, hex address, size) for a
decomp build, e.g.

  symbols.py ~/software/sm64-decomp/build/jp sm64.jp symbols/jp.tsv

Globals come from the ELF's symbol table. IDO leaves static variables out of
it, but keeps them in each object file's .mdebug (ECOFF) debug data; the
linker map places each object's sections, which gives their final addresses.
A static whose name is used by more than one file is written as file.c:name.
Sizes of statics are the distance to the next symbol in the same section."""
import re
import struct
import subprocess
import sys
from pathlib import Path

# ECOFF storage classes of static data, mapped to the ELF section they live in.
STATIC_CLASSES = {2: ".data", 3: ".bss", 13: ".data", 14: ".bss", 15: ".rodata"}
ST_STATIC = 2


def elf_globals(elf):
    listing = subprocess.run(["readelf", "-sW", str(elf)], check=True, capture_output=True, text=True).stdout
    symbols = {}
    for line in listing.splitlines():
        parts = line.split()
        if len(parts) != 8 or not parts[0].endswith(":") or parts[3] not in ("OBJECT", "FUNC", "NOTYPE"):
            continue
        name, address, size = parts[7], int(parts[1], 16), int(parts[2], 0)
        if address >= 0x80000000 and not name.startswith("."):
            symbols.setdefault(name, (address, size))
    return symbols


def section_bases(map_path):
    """{object path: {section: (address, size)}} from a GNU ld map."""
    bases = {}
    pattern = re.compile(r"^ (\.(?:text|data|rodata|bss))\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+\.o)$")
    for line in open(map_path):
        match = pattern.match(line.rstrip("\n"))
        if match:
            section, address, size, obj = match.groups()
            if int(address, 16) >= 0x80000000:
                bases.setdefault(obj, {})[section] = (int(address, 16), int(size, 16))
    return bases


def mdebug_statics(obj_path):
    """[(name, section, offset)] of the static variables in an IDO object."""
    data = Path(obj_path).read_bytes()
    if data[:4] != b"\x7fELF" or data[5] != 2:
        return []
    shoff, = struct.unpack_from(">I", data, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from(">HHH", data, 0x2e)
    sections = [struct.unpack_from(">IIIIIIIIII", data, shoff + i * shentsize) for i in range(shnum)]
    names = sections[shstrndx]
    def section_name(header):
        start = names[4] + header[0]
        return data[start:data.index(b"\0", start)].decode()
    mdebug = next((s for s in sections if section_name(s) == ".mdebug"), None)
    if not mdebug:
        return []
    base = mdebug[4]
    header = struct.unpack_from(">HH" + "I" * 23, data, base)
    magic = header[0]
    if magic != 0x7009:
        return []
    # HDRR offsets are file offsets in IDO's ELF objects.
    cb_sym, cb_ss, ifd_max, cb_fd = header[10], header[16], header[19], header[20]
    statics = []
    for f in range(ifd_max):
        fdr = struct.unpack_from(">IIIIIIIIIIHHIIIIIII", data, cb_fd + f * 72)
        iss_base, isym_base, csym = fdr[2], fdr[4], fdr[5]
        for s in range(csym):
            iss, value, bits = struct.unpack_from(">IiI", data, cb_sym + (isym_base + s) * 12)
            st, sc = bits >> 26, (bits >> 21) & 0x1f
            if st != ST_STATIC or sc not in STATIC_CLASSES:
                continue
            start = cb_ss + iss_base + iss
            name = data[start:data.index(b"\0", start)].decode()
            statics.append((name, STATIC_CLASSES[sc], value))
    return statics


def main():
    if len(sys.argv) != 4:
        sys.exit("usage: symbols.py BUILD_DIR TARGET out.tsv   (e.g. build/jp sm64.jp symbols/jp.tsv)")
    build, target, out_path = Path(sys.argv[1]), sys.argv[2], sys.argv[3]
    root = build.parent.parent
    symbols = elf_globals(build / f"{target}.elf")
    bases = section_bases(build / f"{target}.map")

    found = []  # (name, file, address)
    for obj, sections in bases.items():
        for name, section, offset in mdebug_statics(root / obj):
            if section in sections and name not in symbols:
                found.append((name, Path(obj).stem + ".c", sections[section][0] + offset))
    counts = {}
    for name, _, _ in found:
        counts[name] = counts.get(name, 0) + 1
    for name, source, address in found:
        symbols[name if counts[name] == 1 else f"{source}:{name}"] = (address, 0)

    # Size statics by the distance to the next symbol, within their section.
    ends = sorted({a for sections in bases.values() for a, s in [(v[0] + v[1], 0) for v in sections.values()]})
    addresses = sorted({address for address, _ in symbols.values()})
    import bisect
    for name, (address, size) in list(symbols.items()):
        if size:
            continue
        following = addresses[bisect.bisect_right(addresses, address):]
        end = ends[bisect.bisect_right(ends, address)] if bisect.bisect_right(ends, address) < len(ends) else address + 4
        limit = min([end] + following[:1])
        symbols[name] = (address, max(limit - address, 1))

    with open(out_path, "w") as out:
        for name, (address, size) in sorted(symbols.items(), key=lambda item: (item[1][0], item[0])):
            out.write(f"{name}\t{address:08x}\t{size}\n")
    print(f"{len(symbols)} symbols, {len(found)} of them statics from .mdebug")


if __name__ == "__main__":
    main()
