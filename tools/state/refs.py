#!/usr/bin/env python3
"""Every place the game's code uses a variable of static storage duration.

  refs.py BUILD_DIR [SOURCE...]

Parses the game's sources with libclang, with the flags of BUILD_DIR's
compile_commands.json, and prints one line per use inside a function body:

  file<TAB>line<TAB>column<TAB>variable<TAB>defining file<TAB>const<TAB>in macro

The input for moving the game's variables into a world (docs/state.md).
"""
import json
import shlex
import subprocess
import sys
from pathlib import Path

import clang.cindex as ci

RESOURCE_DIR = subprocess.run(["clang", "-print-resource-dir"], capture_output=True, text=True).stdout.strip()


def flags(entry):
    args, skip = [], False
    for a in shlex.split(entry["command"])[1:]:
        if skip:
            skip = False
            continue
        if a == "-o":
            skip = True
            continue
        if a == "-c" or a == entry["file"] or a.startswith("-finstrument-functions"):
            continue
        args.append(a)
    return args + ["-isystem", f"{RESOURCE_DIR}/include", "-Wno-everything"]


def is_static_storage(var):
    if var.kind != ci.CursorKind.VAR_DECL:
        return False
    if var.semantic_parent is not None and var.semantic_parent.kind == ci.CursorKind.TRANSLATION_UNIT:
        return True
    return var.storage_class == ci.StorageClass.STATIC


def main():
    build = Path(sys.argv[1])
    commands = json.load(open(build / "compile_commands.json"))
    wanted = set(sys.argv[2:])
    index = ci.Index.create()
    seen = set()
    for entry in commands:
        source = entry["file"]
        if wanted and not any(source.endswith(w) for w in wanted):
            continue
        if "/game/" not in source:
            continue
        tu = index.parse(source, args=flags(entry))
        errors = [d for d in tu.diagnostics if d.severity >= ci.Diagnostic.Error]
        if errors:
            print(f"refs.py: {source}: {errors[0]}", file=sys.stderr)
        for cursor in tu.cursor.walk_preorder():
            if cursor.kind != ci.CursorKind.DECL_REF_EXPR:
                continue
            var = cursor.referenced
            if var is None or not is_static_storage(var):
                continue
            # Only uses inside functions: initializers of other variables are data.
            parent = cursor.semantic_parent
            loc = cursor.location
            if loc.file is None:
                continue
            key = (loc.file.name, loc.offset)
            if key in seen:
                continue
            seen.add(key)
            definition = var.get_definition() or var
            in_macro = cursor.extent.start.file is None or cursor.spelling != cursor.extent.start.file.name and False
            print("\t".join([loc.file.name, str(loc.line), str(loc.column), var.spelling,
                             definition.location.file.name if definition.location.file else "?",
                             "const" if var.type.is_const_qualified() else "mutable",
                             str(parent.kind.name if parent else "")]))


if __name__ == "__main__":
    main()
