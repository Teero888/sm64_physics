#!/usr/bin/env python3
"""Generate the lockstep comparator's layout tables.

  layout.py N64_OBJECT NATIVE_OBJECT OUT_HEADER

Both objects are tools/layout/layout_types.c compiled with DWARF: once laid
out as on the N64 (32-bit, 8-byte alignment for 64-bit types), once for the
host. Every struct with a layout_<Name> variable is flattened into leaf
members (scalars and pointers, arrays as a count and a stride), matched by
path between the two layouts, and written as a C table per struct.

A union is read through its first member, except the ones listed in
UNION_VIEWS. Members that exist on one side only are left out; the comparator
handles the one that matters (Object.ptrData) itself.
"""
import re
import subprocess
import sys

UNION_VIEWS = {}


def parse_dies(path):
    text = subprocess.run(["readelf", "--debug-dump=info", "-W", path], check=True,
                          capture_output=True, text=True).stdout
    dies, stack = {}, []
    current = None
    for line in text.splitlines():
        header = re.match(r"\s*<(\d+)><([0-9a-f]+)>: Abbrev Number: (\d+)(?: \((\w+)\))?", line)
        if header:
            level, offset, tag = int(header.group(1)), int(header.group(2), 16), header.group(4)
            if tag is None:  # end of siblings
                del stack[level:]
                current = None
                continue
            current = {"tag": tag, "attrs": {}, "children": []}
            dies[offset] = current
            del stack[level:]
            if stack:
                stack[-1]["children"].append(offset)
            stack.append(current)
            continue
        attr = re.match(r"\s*<[0-9a-f]+>\s+(DW_AT_\w+)\s*:\s*(.*)$", line)
        if attr and current is not None:
            current["attrs"][attr.group(1)] = attr.group(2).strip()
    return dies


def attr_ref(die, name):
    value = die["attrs"].get(name)
    if value is None:
        return None
    match = re.search(r"<0x([0-9a-f]+)>", value)
    return int(match.group(1), 16) if match else None


def attr_int(die, name):
    value = die["attrs"].get(name)
    if value is None:
        return None
    match = re.search(r"\)\s*(-?(?:0x[0-9a-f]+|\d+))", value) or re.search(r"(-?(?:0x[0-9a-f]+|\d+))\s*$", value)
    return int(match.group(1), 0) if match else None


def attr_name(die):
    value = die["attrs"].get("DW_AT_name")
    if value is None:
        return None
    # "(strp) (offset: 0x551): name" or "(string) name"
    return re.sub(r"^\([a-z_0-9]+\)\s*", "", value.split(":")[-1].strip())


class Layout:
    def __init__(self, path):
        self.dies = parse_dies(path)
        self.structs = {}
        for offset, die in self.dies.items():
            if die["tag"] == "DW_TAG_variable":
                name = attr_name(die)
                if name and name.startswith("layout_"):
                    self.structs[name[len("layout_"):]] = self.resolve(attr_ref(die, "DW_AT_type"))

    def resolve(self, offset):
        """Skip typedefs and qualifiers."""
        while offset is not None:
            die = self.dies[offset]
            if die["tag"] in ("DW_TAG_typedef", "DW_TAG_const_type", "DW_TAG_volatile_type"):
                offset = attr_ref(die, "DW_AT_type")
                continue
            return offset
        return None

    def size(self, offset):
        offset = self.resolve(offset)
        if offset is None:
            return 0
        die = self.dies[offset]
        if die["tag"] == "DW_TAG_array_type":
            count = 1
            for dim in self.array_dims(die):
                count *= dim
            return count * self.size(attr_ref(die, "DW_AT_type"))
        return attr_int(die, "DW_AT_byte_size") or 0

    def array_dims(self, die):
        dims = []
        for child in die["children"]:
            sub = self.dies[child]
            if sub["tag"] != "DW_TAG_subrange_type":
                continue
            count = attr_int(sub, "DW_AT_count")
            if count is None:
                upper = attr_int(sub, "DW_AT_upper_bound")
                count = upper + 1 if upper is not None else 0
            dims.append(count)
        return dims

    def leaves(self, offset, path, base, out):
        offset = self.resolve(offset)
        if offset is None:
            return
        die = self.dies[offset]
        tag = die["tag"]
        if tag == "DW_TAG_base_type" or tag == "DW_TAG_enumeration_type":
            size = attr_int(die, "DW_AT_byte_size") or 0
            encoding = die["attrs"].get("DW_AT_encoding", "")
            if "float" in encoding:
                kind = f"F{size * 8}"
            elif "unsigned" in encoding or "boolean" in encoding:
                kind = f"U{size * 8}"
            elif tag == "DW_TAG_enumeration_type":
                kind = f"S{size * 8}"
            else:
                kind = f"S{size * 8}"
            out.append((path, base, kind, 1, 0, None))
        elif tag == "DW_TAG_pointer_type":
            target = self.resolve(attr_ref(die, "DW_AT_type"))
            name = attr_name(self.dies[target]) if target is not None else None
            out.append((path, base, "PTR", 1, 0, name))
        elif tag == "DW_TAG_array_type":
            element = attr_ref(die, "DW_AT_type")
            dims = self.array_dims(die)
            count = 1
            for dim in dims:
                count *= dim
            element_size = self.size(element)
            inner = []
            self.leaves(element, "", 0, inner)
            if len(inner) == 1 and inner[0][3] == 1:
                # Scalar elements: one leaf with a count.
                _, _, kind, _, _, target = inner[0]
                out.append((path, base, kind, count, element_size, target))
            else:
                for i in range(count):
                    self.leaves(element, f"{path}[{i}]", base + i * element_size, out)
        elif tag in ("DW_TAG_structure_type", "DW_TAG_union_type"):
            members = [self.dies[c] for c in die["children"] if self.dies[c]["tag"] == "DW_TAG_member"]
            if tag == "DW_TAG_union_type":
                view = UNION_VIEWS.get(path.split(".")[-1])
                chosen = [m for m in members if attr_name(m) == view] if view else members[:1]
                members = chosen
            for member in members:
                name = attr_name(member) or "_"
                location = attr_int(member, "DW_AT_data_member_location") or 0
                child = f"{path}.{name}" if path else name
                self.leaves(attr_ref(member, "DW_AT_type"), child, base + location, out)


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    n64, native = Layout(sys.argv[1]), Layout(sys.argv[2])
    lines = ["// Generated by tools/layout/layout.py. Do not edit.", "#pragma once", "#include \"compare.h\"", ""]
    for name in sorted(n64.structs):
        n64_leaves, native_leaves = [], []
        n64.leaves(n64.structs[name], "", 0, n64_leaves)
        native.leaves(native.structs[name], "", 0, native_leaves)
        native_by_path = {leaf[0]: leaf for leaf in native_leaves}
        rows = []
        for path, n64_offset, kind, count, n64_stride, target in n64_leaves:
            # Padding and members nothing reads: they hold whatever the
            # memory held before, which differs between the two.
            if re.match(r"(filler|pad|unused)", path.split(".")[-1]):
                continue
            other = native_by_path.get(path)
            if other is None:
                continue
            _, native_offset, native_kind, native_count, native_stride, _ = other
            # uintptr_t: 32 bits on the N64, 64 natively, and it holds addresses.
            if {kind, native_kind} in ({"U32", "U64"}, {"S32", "S64"}):
                native_kind = kind = "PTR"
            if count != native_count or (kind != native_kind and "PTR" not in (kind, native_kind)):
                sys.exit(f"{name}.{path}: {kind}[{count}] on the N64 but {native_kind}[{native_count}] natively")
            if kind != native_kind:
                # A pointer on the N64 and an integer natively (uintptr_t), or
                # the other way round: compare as a pointer.
                kind = "PTR"
            rows.append(f'    {{"{path}", {n64_offset:#x}, {native_offset:#x}, LEAF_{kind}, {count}, '
                        f'{n64_stride}, {native_stride}, {("\"" + target + "\"") if target else "NULL"}}},')
        lines.append(f"static const struct leaf LAYOUT_{name}[] = {{")
        lines += rows
        lines.append("    {NULL, 0, 0, 0, 0, 0, 0, NULL},")
        lines.append("};")
        lines.append(f"#define N64_SIZEOF_{name} {n64.size(n64.structs[name])}")
        lines.append("")
    open(sys.argv[3], "w").write("\n".join(lines))


if __name__ == "__main__":
    main()
