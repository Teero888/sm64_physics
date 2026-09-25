#!/usr/bin/env python3
"""Pointer maps of the state's variables.

  types.py BUILD_DIR [--survey]

For every source of the state section BUILD_DIR compiles, finds the state
variables it defines whose type holds addresses, and writes
game/gen/<version>/pointers/<source>.inc.c: descriptors of those types (as
offsetof expressions, so the compiler lays them out) and a table registering
the variables (platform/pointers.h). Each source includes its file at its
end. Also lists unions where an address shares bytes with other data.
"""
import json
import multiprocessing
import re
import sys
from pathlib import Path

import clang.cindex as ci

sys.path.insert(0, str(Path(__file__).parent))
import refs  # noqa: E402
import rewrite  # noqa: E402

ADDRESS_TYPEDEFS = {"uintptr_t", "intptr_t", "uintptr", "OSMesg"}


def is_address(type_):
    """A type that holds an address of data (not of a function)."""
    t = type_
    while t.kind in (ci.TypeKind.TYPEDEF, ci.TypeKind.ELABORATED):
        if t.kind == ci.TypeKind.TYPEDEF and t.get_declaration().spelling in ADDRESS_TYPEDEFS:
            return True
        t = t.get_canonical() if t.kind == ci.TypeKind.ELABORATED else t.get_declaration().underlying_typedef_type
    if t.kind == ci.TypeKind.POINTER:
        pointee = t.get_pointee().get_canonical()
        return pointee.kind not in (ci.TypeKind.FUNCTIONPROTO, ci.TypeKind.FUNCTIONNOPROTO)
    return False


def element_of(array):
    """An array type's element type, as declared (typedefs kept)."""
    if array.kind in (ci.TypeKind.CONSTANTARRAY, ci.TypeKind.INCOMPLETEARRAY):
        return array.element_type
    if array.kind == ci.TypeKind.ELABORATED:
        return element_of(array.get_named_type())
    return array.get_canonical().element_type


class Emitter:
    """Descriptors for one source, as C."""

    def __init__(self, prefix="sHostType"):
        self.lines = []
        self.named = {}  # canonical spelling of a named type -> descriptor name
        self.count = 0
        self.unions = []
        self.prefix = prefix

    def descriptor(self, type_, expr):
        """The descriptor's name for type_ (a record), whose C type is expr;
        None if it holds no addresses."""
        canonical = type_.get_canonical()
        key = canonical.spelling
        named = "(anonymous" not in key and "(unnamed" not in key
        if named and key in self.named:
            return self.named[key]
        if named:
            expr = key
        fields = []
        self.fields_of(canonical, expr, expr, fields, "")
        if not fields:
            if named:
                self.named[key] = None
            return None
        name = f"{self.prefix}{self.count}"
        self.count += 1
        if named:
            self.named[key] = name
        self.lines.append(f"static const struct host_field {name}Fields[] = {{")
        for offset, sub, count, stride in fields:
            self.lines.append(f"    {{ {offset}, {sub}, {count}, {stride} }},")
        self.lines.append("};")
        self.lines.append(f"static const struct host_type {name} = {{ sizeof({expr}), "
                          f"sizeof({name}Fields) / sizeof({name}Fields[0]), {name}Fields }};")
        return name

    def fields_of(self, record, expr, base, out, prefix):
        """Fields of record (C type base), named with prefix for anonymous
        members, that hold addresses."""
        is_union = record.get_declaration().kind == ci.CursorKind.UNION_DECL
        kinds = set()
        for field in record.get_fields():
            if field.is_bitfield():
                continue
            # A member without a name (C11 anonymous struct or union): its
            # members are the record's.
            if not field.spelling:
                self.fields_of(field.type.get_canonical(), expr, base, out, prefix)
                continue
            member = prefix + field.spelling
            entry = self.member(field.type, base, member)
            kinds.add(entry is not None)
            if entry:
                out.append(entry)
        if is_union and kinds == {True, False}:
            self.unions.append(base)

    def member(self, type_, base, member):
        """(offset, descriptor, count, stride) for a member of type_, or None."""
        offset = f"offsetof({base}, {member})"
        count, element, index = 1, type_, ""
        while element.get_canonical().kind == ci.TypeKind.CONSTANTARRAY:
            count *= element.get_canonical().element_count
            element = element_of(element)
            index += "[0]"
        if element.get_canonical().kind == ci.TypeKind.INCOMPLETEARRAY:
            return None
        stride = f"sizeof((({base} *) 0)->{member}{index})"
        if is_address(element):
            return (offset, "NULL", count, stride)
        if element.get_canonical().kind == ci.TypeKind.RECORD:
            sub = self.descriptor(element, f"__typeof__((({base} *) 0)->{member}{index})")
            if sub:
                return (offset, "&" + sub, count, stride)
        return None


HOST_TYPE_OF = re.compile(r"HOST_TYPE_OF\((\w+)\)")


def shared_types(tu, source):
    """{name: (definition header, C text of its descriptor)} for the types the
    source marks memory with (HOST_TYPE_OF(name): a struct tag or typedef)."""
    names = set(HOST_TYPE_OF.findall(Path(source).read_text(errors="replace")))
    names.discard("name")
    found = {}
    if not names:
        return found
    for cursor in tu.cursor.walk_preorder():
        if cursor.spelling not in names or cursor.spelling in found:
            continue
        if cursor.kind in (ci.CursorKind.STRUCT_DECL, ci.CursorKind.UNION_DECL) and cursor.is_definition():
            # As C spells the type: an unnamed struct its typedef names is
            # spelled by that name.
            spelled = cursor.type.spelling
        elif cursor.kind == ci.CursorKind.TYPEDEF_DECL:
            spelled = cursor.spelling
        else:
            continue
        emit = Emitter(prefix=f"sHostType_{cursor.spelling}_")
        desc = emit.descriptor(cursor.type, spelled)
        if desc is None:
            fields = f"static const struct host_field sHostType_{cursor.spelling}_None[1];"
            text = (f"const struct host_type gHostType_{cursor.spelling} = {{ sizeof({spelled}), 0, NULL }};\n")
        else:
            text = "\n".join(emit.lines) + (f"\nconst struct host_type gHostType_{cursor.spelling} = {{ "
                                           f"sizeof({spelled}), sizeof({desc}Fields) / sizeof({desc}Fields[0]), "
                                           f"{desc}Fields }};\n")
        found[cursor.spelling] = (cursor.location.file.name, text)
    missing = names - set(found)
    if missing:
        print(f"types.py: {source}: no type named {sorted(missing)}", file=sys.stderr)
    return found


def generate(entry):
    """(source, text, unions) for one source."""
    source = entry["file"]
    if "/n64stack/" in source:
        source = Path(source).read_text().splitlines()[1].split('"')[1]
    tu = ci.Index.create().parse(source, args=refs.flags(entry))
    emit = Emitter()
    variables, seen = [], set()
    for cursor in tu.cursor.get_children():
        # Definitions, tentative ones (no initializer) included.
        if cursor.kind != ci.CursorKind.VAR_DECL or cursor.storage_class == ci.StorageClass.EXTERN:
            continue
        if rewrite.is_const(cursor.type) or cursor.spelling in seen:
            continue
        name = cursor.spelling
        # The complete type: a later declaration may give the array's size.
        if cursor.type.get_canonical().kind == ci.TypeKind.INCOMPLETEARRAY:
            definition = cursor.get_definition()
            if definition is not None:
                cursor = definition
        seen.add(name)
        count, element, index = 1, cursor.type, ""
        while element.get_canonical().kind in (ci.TypeKind.CONSTANTARRAY, ci.TypeKind.INCOMPLETEARRAY):
            canonical = element.get_canonical()
            if canonical.kind == ci.TypeKind.INCOMPLETEARRAY:
                count = None
            elif count is not None:
                count *= canonical.element_count
            element = element_of(element)
            index += "[0]"
        count_expr = str(count) if count is not None else f"sizeof({name}) / sizeof({name}{index})"
        if is_address(element):
            variables.append((name, "&gHostTypeAddress", count_expr))
        elif element.get_canonical().kind == ci.TypeKind.RECORD:
            desc = emit.descriptor(element, f"__typeof__({name}{index})")
            if desc:
                variables.append((name, "&" + desc, count_expr))
    lines = ["// Generated by tools/state/types.py: the addresses among this source's",
             "// state variables (platform/pointers.h, docs/state.md). Do not edit.",
             "#include <stddef.h>", '#include "pointers.h"', ""]
    if variables:
        lines += emit.lines + [""]
        lines.append("static const struct host_variable sHostVariables[] HOST_VARIABLE_TABLE = {")
        lines += [f"    {{ &{n}, {d}, {c} }}," for n, d, c in variables]
        lines += ["};", "HOST_REGISTER_VARIABLES(sHostVariables)", ""]
    shared = shared_types(tu, source)
    # A type the source itself defines can only be described there.
    local = {name: text for name, (header, text) in shared.items() if header.endswith(".c")}
    if local:
        lines += ["// Types this source marks memory with (HOST_TYPE_OF), defined here."]
        lines += [local[name] for name in sorted(local)]
    shared = {name: value for name, value in shared.items() if name not in local}
    return source, "\n".join(lines), emit.unions, len(variables), shared, sorted(local)


def main():
    build = Path(sys.argv[1])
    root = Path(__file__).resolve().parent.parent.parent
    version = re.search(r"SM64_VERSION:STRING=(\w+)", (build / "CMakeCache.txt").read_text()).group(1)
    entries = rewrite.state_sources(build)
    with multiprocessing.Pool() as pool:
        results = pool.map(generate, entries, chunksize=4)
    total, unions, shared, local = 0, set(), {}, set()
    for source, text, source_unions, count, types, local_types in results:
        shared.update(types)
        local.update(local_types)
        relative = Path(source).resolve().relative_to(root)
        out = root / "game" / "gen" / version / "pointers" / f"{relative}.inc.c"
        out.parent.mkdir(parents=True, exist_ok=True)
        if not out.exists() or out.read_text() != text:
            out.write_text(text)
        # The source includes its map at its end.
        include = f'#include "pointers/{relative}.inc.c"'
        src = Path(source)
        body = src.read_text(errors="surrogateescape")
        if include not in body:
            src.write_text(body.rstrip("\n") + "\n\n// Library: its variables' addresses (tools/state/types.py).\n"
                           + include + "\n", errors="surrogateescape")
        total += count
        unions |= set(source_unions)
    # The types memory is marked with, for every source (platform/pointers.h).
    headers = sorted({header for header, _ in shared.values()})
    out = root / "game" / "gen" / version / "pointers"
    body = ["// Generated by tools/state/types.py: the types the game marks the memory",
            "// it hands out with (HOST_TYPE_OF, platform/pointers.h). Do not edit.",
            "#include <stddef.h>", "#include <ultra64.h>", '#include "types.h"']
    game = root / "game"

    def include_path(header):
        relative = Path(header).resolve().relative_to(game)
        return str(relative.relative_to("include")) if relative.parts[0] == "include" else str(relative)

    body += [f'#include "{include_path(h)}"' for h in headers] + ['#include "pointers.h"', ""]
    body += [shared[name][1] for name in sorted(shared)]
    declarations = ["// Generated by tools/state/types.py. Do not edit.", "#pragma once", ""]
    declarations += [f"extern const struct host_type gHostType_{name};" for name in sorted(set(shared) | local)]
    for path, text in ((out / "types.c", "\n".join(body) + "\n"), (out / "types.h", "\n".join(declarations) + "\n")):
        if not path.exists() or path.read_text() != text:
            path.write_text(text)
    print(f"{total} variables in {len(results)} sources, {len(shared)} types for memory")
    for u in sorted(unions):
        print("union with addresses and other data:", u)


if __name__ == "__main__":
    main()
