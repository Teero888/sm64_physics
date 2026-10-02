#!/usr/bin/env python3
"""The library's object without the zeros of its largest sections.

  sparse.py OBJCOPY OBJECT OUT_OBJECT OUT_C

Most of the state (sm64_state: the memory pools, framebuffers, heaps) and of
the constant data (the textures' stand-ins, tools/rom_stubs.py) is zeros, which
the file held as they are: about 9 MB of a version's library. OUT_OBJECT is
OBJECT with both sections as memory the loader only reserves (NOBITS,
uninitialized data), and OUT_C restores what they held before anything uses
them, in a constructor: their bytes that are not zero, and the addresses their
relocations put there (absolute ones, and the jump tables' relative ones),
computed from symbols the linker resolves as before. The memory then holds
exactly what the loader made of the full sections.

The constant data is renamed (sm64_rodata), so it stays a section of its own
in the library rather than one more piece of .rodata, and becomes writable,
for the constructor.

Reads ELF objects and, for Windows builds, COFF ones (as init_pointers.py).
"""
import sys

# This directory's types.py is a tool, not the standard library's types.
_here = __file__.replace("\\", "/").rpartition("/")[0].rstrip("/")
sys.path = [p for p in sys.path if p.replace("\\", "/").rstrip("/") != _here]

import struct
import subprocess

# Absolute 64-bit addresses, and 32-bit ones relative to their place
ABS64, REL32 = "abs64", "rel32"


class Section:
    def __init__(self, name, data):
        self.name, self.data = name, data
        self.relocations = []  # (offset, kind, symbol index, addend: relative ones from their place)
        self.relocation_section = None  # ELF: the section holding them


class Symbol:
    def __init__(self, name, section, value, is_global):
        self.name, self.section, self.value, self.is_global = name, section, value, is_global


def elf(data, wanted):
    """{name: Section} of `wanted`, the symbols, the section names by index."""
    shoff, = struct.unpack_from("<Q", data, 0x28)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 0x3a)
    headers = [struct.unpack_from("<IIQQQQIIQQ", data, shoff + i * shentsize) for i in range(shnum)]
    strtab = headers[shstrndx]

    def string(table, offset):
        start = table[4] + offset
        return data[start:data.index(b"\0", start)].decode("latin-1")

    names = [string(strtab, h[0]) for h in headers]
    sections = {}
    for i, h in enumerate(headers):
        if names[i] in wanted:
            sections[names[i]] = (i, Section(names[i], bytearray(data[h[4]:h[4] + h[5]])))
    symtab = next(h for h in headers if h[1] == 2)  # SHT_SYMTAB
    symbols = []
    for at in range(symtab[4], symtab[4] + symtab[5], 24):
        name, info, other, shndx, value, size = struct.unpack_from("<IBBHQQ", data, at)
        symbols.append(Symbol(string(headers[symtab[6]], name), shndx, value, (info >> 4) in (1, 2)))
    for i, h in enumerate(headers):
        if h[1] != 4:  # SHT_RELA
            continue
        target = next((s for (index, s) in sections.values() if index == h[7]), None)
        if not target:
            continue
        target.relocation_section = names[i]
        for at in range(h[4], h[4] + h[5], 24):
            offset, info, addend = struct.unpack_from("<QQq", data, at)
            kind = info & 0xffffffff
            if kind == 1:  # R_X86_64_64: S + A
                target.relocations.append((offset, ABS64, info >> 32, addend))
            elif kind == 2:  # R_X86_64_PC32: S + A - P
                target.relocations.append((offset, REL32, info >> 32, addend))
            else:
                sys.exit(f"sparse.py: relocation type {kind} in {target.name}")
    return {n: s for n, (i, s) in sections.items()}, symbols, names


def coff(data, wanted):
    """The same for an x86-64 COFF object (MinGW's relocatable link writes it
    behind a DOS and PE header); addends are in the data."""
    base = struct.unpack_from("<I", data, 0x3c)[0] + 4 if data[:2] == b"MZ" else 0
    count = struct.unpack_from("<H", data, base + 2)[0]
    symbol_table, symbol_count = struct.unpack_from("<II", data, base + 8)
    strings = symbol_table + 18 * symbol_count

    def long_name(offset):
        return data[strings + offset:data.index(b"\0", strings + offset)].decode("latin-1")

    names, headers = [None], [None]  # 1-based, as the symbols number them
    for i in range(count):
        h = base + 20 + struct.unpack_from("<H", data, base + 16)[0] + 40 * i
        raw = data[h:h + 8]
        names.append(long_name(int(raw[1:].rstrip(b"\0"))) if raw[:1] == b"/" else raw.rstrip(b"\0").decode("latin-1"))
        headers.append(h)
    symbols, i = [], 0
    while i < symbol_count:
        at = symbol_table + 18 * i
        raw = data[at:at + 8]
        name = long_name(struct.unpack_from("<I", raw, 4)[0]) if raw[:4] == b"\0\0\0\0" else raw.rstrip(b"\0").decode("latin-1")
        value, section = struct.unpack_from("<Ih", data, at + 8)
        storage, aux = data[at + 16], data[at + 17]
        symbols.append(Symbol(name, section, value, storage == 2))
        symbols.extend([Symbol("", 0, 0, False)] * aux)
        i += 1 + aux
    sections = {}
    for index in range(1, count + 1):
        if names[index] not in wanted:
            continue
        h = headers[index]
        size, pointer, relocations, _, n = struct.unpack_from("<IIIIH", data, h + 16)
        if struct.unpack_from("<I", data, h + 36)[0] & 0x01000000:  # more than 65535: the first holds the count
            n = struct.unpack_from("<I", data, relocations)[0]
            relocations, n = relocations + 10, n - 1
        section = Section(names[index], bytearray(data[pointer:pointer + size]))
        for r in range(n):
            offset, symbol, kind = struct.unpack_from("<IIH", data, relocations + 10 * r)
            if kind == 1:  # IMAGE_REL_AMD64_ADDR64: S + addend
                addend = struct.unpack_from("<q", section.data, offset)[0]
                section.relocations.append((offset, ABS64, symbol, addend))
            elif kind == 4:  # IMAGE_REL_AMD64_REL32: S + addend - (P + 4)
                addend = struct.unpack_from("<i", section.data, offset)[0]
                section.relocations.append((offset, REL32, symbol, addend - 4))
            else:
                sys.exit(f"sparse.py: relocation type {kind} in {section.name}")
        sections[names[index]] = section
    return sections, symbols, names


def runs(data, gap=16):
    """(offset, length) of the bytes that are not zero; zeros shorter than
    `gap` between two of them are kept, for fewer, longer runs."""
    out, i, n = [], 0, len(data)
    while i < n:
        if data[i] == 0:
            i += 1
            continue
        start = end = i
        while i < n:
            if data[i]:
                end = i = i + 1
            elif i - end < gap:
                i += 1
            else:
                break
        out.append((start, end - start))
    return out


def main():
    objcopy, obj, out_obj, out_c = sys.argv[1:5]
    with open(obj, "rb") as f:
        data = f.read()
    is_elf = data[:4] == b"\x7fELF"
    constants = ".rodata" if is_elf else ".rdata"
    sections, symbols, names = (elf if is_elf else coff)(data, {"sm64_state", constants})

    # Each relocation's target as a global symbol of its section (an anchor)
    # plus an offset: the C below has a table of the anchors' addresses, which
    # the loader relocates, and indices into it.
    anchors = {}
    for s in symbols:
        if s.is_global and s.section > 0 and s.name and s.section not in anchors:
            anchors[s.section] = s
    anchor_names = []

    def anchor_index(name):
        if name not in anchor_names:
            anchor_names.append(name)
        return anchor_names.index(name)

    def target(index, addend):
        s = symbols[index]
        if s.section <= 0:  # defined outside the object
            return s.name, addend
        anchor = anchors.get(s.section)
        if not anchor:
            sys.exit(f"sparse.py: no global symbol in section {names[s.section]} to reach {s.name} by")
        return anchor.name, s.value + addend - anchor.value

    body, restore = [], []
    for section_name in ("sm64_state", constants):
        section = sections[section_name]
        tag = "state" if section_name == "sm64_state" else "constants"
        index = names.index(section_name)
        base = anchors.get(index)
        if not base:
            sys.exit(f"sparse.py: no global symbol in {section_name}")
        tables = {ABS64: [], REL32: []}
        for offset, kind, symbol, addend in section.relocations:
            width = 8 if kind == ABS64 else 4
            section.data[offset:offset + width] = bytes(width)
            name, value = target(symbol, addend)
            if not -2**31 <= value < 2**31:
                sys.exit(f"sparse.py: offset {value} from {name}")
            tables[kind].append((offset, anchor_index(name), value))
        spans = runs(section.data)
        payload = b"".join(bytes(section.data[o:o + n]) for o, n in spans)

        def array(kind, name, values, per_line):
            values = list(values) or [0]  # (C has no empty arrays)
            body.append(f"static const {kind} {tag}_{name}[] = {{")
            for k in range(0, len(values), per_line):
                body.append("    " + ",".join(str(v) for v in values[k:k + per_line]) + ",")
            body.append("};")

        array("uint32_t", "runs", [x for span in spans for x in span], 16)
        array("unsigned char", "bytes", payload, 32)
        for kind, label in ((ABS64, "absolute"), (REL32, "relative")):
            array("uint32_t", f"{label}_at", (o for o, _, _ in tables[kind]), 16)
            array("uint16_t", f"{label}_anchor", (a for _, a, _ in tables[kind]), 24)
            array("int32_t", f"{label}_offset", (v for _, _, v in tables[kind]), 16)
        restore.append(f"    restore({anchor_index(base.name)}, {-base.value}, {tag}_runs, {len(spans)}, {tag}_bytes,\n"
                       f"            {tag}_absolute_at, {tag}_absolute_anchor, {tag}_absolute_offset, {len(tables[ABS64])},\n"
                       f"            {tag}_relative_at, {tag}_relative_anchor, {tag}_relative_offset, {len(tables[REL32])});")
        print(f"{section_name}: {len(section.data)} bytes as {len(payload)} in {len(spans)} runs, "
              f"{len(tables[ABS64])} absolute and {len(tables[REL32])} relative addresses")

    out = ["// Generated by tools/state/sparse.py: what the sections it made memory the",
           "// loader only reserves held, restored before anything uses them.",
           "#include <stdint.h>", "#include <string.h>", ""]
    out += [f"extern char {name}[];" for name in anchor_names]
    out += ["static char *const anchors[] = {"] + [f"    {name}," for name in anchor_names] + ["};", ""]
    out += body
    out += ["",
            "static void restore(uint16_t base, int32_t base_offset, const uint32_t *runs, uint32_t run_count,",
            "                    const unsigned char *bytes, const uint32_t *absolute_at, const uint16_t *absolute_anchor,",
            "                    const int32_t *absolute_offset, uint32_t absolute_count, const uint32_t *relative_at,",
            "                    const uint16_t *relative_anchor, const int32_t *relative_offset, uint32_t relative_count) {",
            "    // (as integers: the offsets reach outside the anchors' objects)",
            "    char *const section = (char *) ((uintptr_t) anchors[base] + base_offset);",
            "    for (uint32_t i = 0; i < run_count; ++i) {",
            "        memcpy(section + runs[2 * i], bytes, runs[2 * i + 1]);",
            "        bytes += runs[2 * i + 1];",
            "    }",
            "    for (uint32_t i = 0; i < absolute_count; ++i) {",
            "        const uint64_t value = (uintptr_t) anchors[absolute_anchor[i]] + absolute_offset[i];",
            "        memcpy(section + absolute_at[i], &value, sizeof(value));",
            "    }",
            "    for (uint32_t i = 0; i < relative_count; ++i) {",
            "        const int32_t value = (int32_t) ((uintptr_t) anchors[relative_anchor[i]] + relative_offset[i]",
            "                                         - (uintptr_t) (section + relative_at[i]));",
            "        memcpy(section + relative_at[i], &value, sizeof(value));",
            "    }",
            "}",
            "",
            "// Before every other constructor, so nothing reads them first (and named,",
            "// for platform/world.c to keep this in a static library).",
            "__attribute__((constructor(101))) void sm64_sparse_restore(void) {"]
    out += restore + ["}"]
    with open(out_c, "w") as f:
        f.write("\n".join(out) + "\n")

    # Their relocations go (the C above makes them), then the data: a
    # section's flags without contents make it memory the loader reserves.
    # (ELF holds relocations as sections; GNU's objcopy removes COFF's with
    # an option LLVM's lacks, and LLVM's needs the type set as well.)
    if is_elf:
        drop = [f"--remove-section={s.relocation_section}" for s in sections.values() if s.relocation_section]
    else:
        drop = ["--remove-relocations=sm64_state", f"--remove-relocations={constants}"]
    subprocess.run([objcopy, *drop, "--rename-section", "sm64_state=sm64_state,alloc",
                    "--rename-section", f"{constants}=sm64_rodata,alloc", obj, out_obj], check=True)
    if is_elf:
        if not is_nobits(out_obj, ("sm64_state", "sm64_rodata")):
            subprocess.run([objcopy, "--set-section-type=sm64_state=8", "--set-section-type=sm64_rodata=8",
                            out_obj], check=True)
        if not is_nobits(out_obj, ("sm64_state", "sm64_rodata")):
            sys.exit(f"sparse.py: {objcopy} left the sections' contents in the file")


def is_nobits(path, wanted):
    with open(path, "rb") as f:
        data = f.read()
    shoff, = struct.unpack_from("<Q", data, 0x28)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 0x3a)
    names = struct.unpack_from("<Q", data, shoff + shstrndx * shentsize + 24)[0]
    found = 0
    for i in range(shnum):
        name, kind = struct.unpack_from("<II", data, shoff + i * shentsize)
        if data[names + name:data.index(b"\0", names + name)].decode("latin-1") in wanted:
            if kind != 8:  # SHT_NOBITS
                return False
            found += 1
    return found == len(wanted)

if __name__ == "__main__":
    main()
