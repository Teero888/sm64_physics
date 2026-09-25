#!/usr/bin/env python3
"""Make every use of the game's state go through the current world.

  rewrite.py BUILD_DIR [--dry-run]

The game's state is the variables linked into the sm64_state section
(platform/state.ld). Each world has its own copy of them; code reaches the
current world's through WORLD(x) (platform/world.h). This rewrites, in the
sources BUILD_DIR compiles, every use of such a variable inside a function
body from `x` to `WORLD(x)`, where its name is spelled: in the function, or in
the macro definition it comes from. Initializers of variables are data and
stay as they are: a new world's pointers are relocated instead.

A variable is state if it is not const and is defined in a source of the
state section: globals are the section's symbols in BUILD_DIR/sm64_game.o,
statics are those of the sources linked into it. Uses that cannot be
rewritten where they are spelled (token pasting) are listed for review.
"""
import json
import multiprocessing
import re
import subprocess
import sys
from ctypes import byref, c_uint
from pathlib import Path

import clang.cindex as ci

sys.path.insert(0, str(Path(__file__).parent))
import refs  # noqa: E402


def state_globals(build):
    """Global symbols in the sm64_state section of the library's object."""
    out = subprocess.run(["readelf", "-sW", str(build / "sm64_game.o")], check=True, capture_output=True,
                         text=True).stdout
    sections = subprocess.run(["readelf", "-SW", str(build / "sm64_game.o")], check=True, capture_output=True,
                              text=True).stdout
    index = next(int(m.group(1)) for m in re.finditer(r"\[\s*(\d+)\]\s+(\S+)", sections) if m.group(2) == "sm64_state")
    names = set()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 8 and parts[4] == "GLOBAL" and parts[6] == str(index) and parts[3] == "OBJECT":
            names.add(parts[7])
    return names


def spelling(location):
    file, line, column, offset = ci.c_object_p(), c_uint(), c_uint(), c_uint()
    ci.conf.lib.clang_getSpellingLocation(location, byref(file), byref(line), byref(column), byref(offset))
    return (ci.File(file).name if file else None), offset.value


def state_sources(build):
    """The sources linked into the state section, from the group object libraries."""
    commands = json.load(open(build / "compile_commands.json"))
    sources = []
    for entry in commands:
        output = entry.get("output") or re.search(r"-o (\S+)", entry["command"]).group(1)
        if re.search(r"sm64_(main|overlay|level_data)_objects\.dir", output):
            sources.append(entry)
    return sources


GLOBALS = set()


def is_const(type_):
    """A const object: const itself, or an array of const elements."""
    while True:
        if type_.is_const_qualified():
            return True
        if type_.kind in (ci.TypeKind.CONSTANTARRAY, ci.TypeKind.INCOMPLETEARRAY, ci.TypeKind.VARIABLEARRAY):
            type_ = type_.element_type
        elif type_.kind == ci.TypeKind.ELABORATED or type_.kind == ci.TypeKind.TYPEDEF:
            canonical = type_.get_canonical()
            if canonical == type_:
                return False
            type_ = canonical
        else:
            return False


def scan(entry):
    """(edits, problems) for one translation unit."""
    source = entry["file"]
    if "/n64stack/" in source:
        source = Path(source).read_text().splitlines()[1].split('"')[1]
    tu = ci.Index.create().parse(source, args=refs.flags(entry))
    errors = [str(d) for d in tu.diagnostics if d.severity >= ci.Diagnostic.Error]
    edits, problems = set(), []
    if errors:
        return edits, [f"{source}: {errors[0]}"]

    def visit(cursor, in_function):
        for child in cursor.get_children():
            kind = child.kind
            inside = in_function or (kind == ci.CursorKind.FUNCTION_DECL and child.is_definition())
            if kind == ci.CursorKind.VAR_DECL and in_function and child.storage_class == ci.StorageClass.STATIC:
                # A function's static variable: its initializer is data.
                inside = False
            if kind == ci.CursorKind.DECL_REF_EXPR and in_function:
                var = child.referenced
                if var is not None and var.kind == ci.CursorKind.VAR_DECL and is_state(var):
                    file, offset = spelling(child.location)
                    if file is None:
                        problems.append(f"{source}:{child.location.line}: {var.spelling} is not spelled in a file")
                    else:
                        edits.add((file, offset, var.spelling))
            visit(child, inside)

    def is_state(var):
        if is_const(var.type):
            return False
        linkage = var.linkage
        parent = var.semantic_parent
        if parent is not None and parent.kind == ci.CursorKind.FUNCTION_DECL:
            return var.storage_class == ci.StorageClass.STATIC
        if linkage == ci.LinkageKind.INTERNAL:
            return True
        return var.spelling in GLOBALS

    visit(tu.cursor, False)
    return edits, problems


def init(globals_):
    GLOBALS.update(globals_)


def main():
    build = Path(sys.argv[1])
    dry = "--dry-run" in sys.argv
    globals_ = state_globals(build)
    entries = state_sources(build)
    with multiprocessing.Pool(initializer=init, initargs=(globals_,)) as pool:
        results = pool.map(scan, entries, chunksize=4)
    edits, problems = set(), []
    for e, p in results:
        edits |= e
        problems += p
    by_file = {}
    for file, offset, name in edits:
        by_file.setdefault(file, []).append((offset, name))
    changed = 0
    for file, spots in sorted(by_file.items()):
        text = Path(file).read_bytes()
        for offset, name in sorted(set(spots), reverse=True):
            word = name.encode()
            if text[offset:offset + len(word)] != word:
                problems.append(f"{file}@{offset}: expected {name}")
                continue
            if text[max(0, offset - 6):offset] == b"WORLD(":
                continue
            text = text[:offset] + b"WORLD(" + word + b")" + text[offset + len(word):]
            changed += 1
        if not dry:
            Path(file).write_bytes(text)
    print(f"{len(globals_)} state globals, {len(entries)} sources, {changed} uses in {len(by_file)} files")
    for p in problems:
        print("problem:", p)


if __name__ == "__main__":
    main()
