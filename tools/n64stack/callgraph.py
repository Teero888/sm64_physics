"""The N64 game's functions and call graph, from the decomp's matching build.

Functions, static ones included, come from each object's .mdebug procedure
descriptors (name, offset in the object's .text, stack frame size), placed by
the linker map. Direct calls are jal (and j to another function's start: a
tail call). An indirect call (jalr) can reach any function whose address the
game takes: one that appears as a word in any section of the ELF, or that
code builds with lui and addiu/ori. That over-approximates the real graph,
which only costs time in the stack model.
"""
import re
import struct
from collections import defaultdict, namedtuple
from pathlib import Path

ST_PROC, ST_STATIC_PROC = 6, 14
SHT_PROGBITS = 1
SHF_EXECINSTR = 4

Function = namedtuple("Function", "object name address size frame")
Section = namedtuple("Section", "name type flags address offset size")


def read_elf(data):
    shoff, = struct.unpack_from(">I", data, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from(">HHH", data, 0x2e)
    sections = [struct.unpack_from(">IIIIIIIIII", data, shoff + i * shentsize) for i in range(shnum)]
    names = sections[shstrndx]

    def section_name(header):
        start = names[4] + header[0]
        return data[start:data.index(b"\0", start)].decode()

    return [Section(section_name(s), *s[1:6]) for s in sections]


def procedures(obj_path):
    """[(name, offset in .text, frame size)] of the functions in an IDO object."""
    data = Path(obj_path).read_bytes()
    mdebug = next((s for s in read_elf(data) if s.name == ".mdebug"), None)
    if mdebug is None:  # assembly
        return []
    header = struct.unpack_from(">HH" + "I" * 23, data, mdebug.offset)
    cb_pd, cb_sym, cb_ss, ifd_max, cb_fd = header[8], header[10], header[16], header[19], header[20]
    result = []
    for f in range(ifd_max):
        fdr = struct.unpack_from(">IIIIIIIIIIHHIIIIIII", data, cb_fd + f * 72)
        iss_base, isym_base, ipd_first, cpd = fdr[2], fdr[4], fdr[10], fdr[11]
        for p in range(cpd):
            pdr = struct.unpack_from(">IiiiiiiiihhiiI", data, cb_pd + (ipd_first + p) * 52)
            adr, isym, frame = pdr[0], pdr[1], pdr[8]
            iss, _, bits = struct.unpack_from(">IiI", data, cb_sym + (isym_base + isym) * 12)
            if bits >> 26 not in (ST_PROC, ST_STATIC_PROC):
                continue
            start = cb_ss + iss_base + iss
            result.append((data[start:data.index(b"\0", start)].decode(), adr, frame))
    return result


def text_sections(map_path):
    """[(object path, address, size)] of every object's .text in a GNU ld map.
    Archive members (libgoddard.a(x.o)) are named by their object in the
    archive's directory."""
    pattern = re.compile(r"^ \.text\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+)$")
    member = re.compile(r"^(.*)/lib(\w+)\.a\((\S+\.o)\)$")
    result = []
    for line in open(map_path):
        match = pattern.match(line.rstrip("\n"))
        if not match:
            continue
        address, size, obj = int(match.group(1), 16), int(match.group(2), 16), match.group(3)
        archive = member.match(obj)
        if archive:
            obj = f"{archive.group(1)}/src/{archive.group(2)}/{archive.group(3)}"
        if address >= 0x80000000 and size:
            result.append((obj, address, size))
    return result


def functions(build, target):
    """Every function of the game with a procedure descriptor."""
    root = Path(build).parent.parent
    result = []
    for obj, base, size in text_sections(Path(build) / f"{target}.map"):
        path = root / obj
        if not path.exists():
            continue
        procs = sorted(procedures(path), key=lambda p: p[1])
        for i, (name, offset, frame) in enumerate(procs):
            end = procs[i + 1][1] if i + 1 < len(procs) else size
            result.append(Function(obj, name, base + offset, end - offset, frame))
    return result


def call_graph(build, target, indirect_reach=lambda caller, callee: True):
    """(functions, {function: set of callees}), indirect calls resolved to
    every address-taken function for which indirect_reach(caller, callee)."""
    data = Path(build, f"{target}.elf").read_bytes()
    sections = read_elf(data)
    funcs = functions(build, target)
    starts = defaultdict(list)
    for f in funcs:
        starts[f.address].append(f)

    taken = set()
    for s in sections:
        if s.type != SHT_PROGBITS:
            continue
        chunk = data[s.offset:s.offset + s.size // 4 * 4]
        for (word,) in struct.iter_unpack(">I", chunk):
            if word in starts:
                taken.update(starts[word])

    code = [s for s in sections if s.type == SHT_PROGBITS and s.flags & SHF_EXECINSTR and s.address >= 0x80000000]
    graph, indirect = {}, set()
    for f in funcs:
        section = next(s for s in code if s.address <= f.address < s.address + s.size)
        words = struct.unpack_from(f">{f.size // 4}I", data, section.offset + f.address - section.address)
        callees = set()
        high = {}  # register -> upper half from lui
        for i, word in enumerate(words):
            op = word >> 26
            pc = f.address + 4 * i
            if op in (2, 3):  # j, jal
                target_address = (pc + 4) & 0xf0000000 | (word & 0x03ffffff) << 2
                if target_address != f.address:
                    callees.update(starts.get(target_address, ()))
            elif op == 0 and word & 0x3f == 9:  # jalr
                indirect.add(f)
            elif op == 15:  # lui
                high[word >> 16 & 31] = (word & 0xffff) << 16
            elif op in (9, 13):  # addiu, ori
                rs = word >> 21 & 31
                if rs in high:
                    low = word & 0xffff
                    if op == 9 and low & 0x8000:
                        low -= 0x10000
                    taken.update(starts.get((high[rs] + low) & 0xffffffff, ()))
        graph[f] = callees
    for f in indirect:
        graph[f] |= {g for g in taken if indirect_reach(f, g)}
    return funcs, graph


def ancestors(graph, targets):
    """The functions from which a call chain can reach one of targets."""
    callers = defaultdict(set)
    for caller, callees in graph.items():
        for callee in callees:
            callers[callee].add(caller)
    seen, todo = set(), list(targets)
    while todo:
        for caller in callers[todo.pop()]:
            if caller not in seen:
                seen.add(caller)
                todo.append(caller)
    return seen - set(targets)
