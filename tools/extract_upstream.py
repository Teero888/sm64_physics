#!/usr/bin/env python3
"""Copy selected complete functions verbatim out of mixed upstream files.

No replacement bodies, regex substitutions, or source patches are applied.
The manifest records both the original file and each selected byte range.
"""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
EXTRACTIONS = {
    "terrain_load.c": {
        "source": "src/engine/surface_load.c",
        "headers": ["host/terrain_state.h"],
        "functions": ["alloc_surface_node", "alloc_surface", "add_surface_to_cell",
                      "min_3", "max_3", "lower_cell_index", "upper_cell_index",
                      "add_surface", "read_surface_data", "surface_has_force",
                      "surf_has_no_cam_collision"],
        "footer": "host/terrain.c",
    },
    "terrain_queries.c": {
        "source": "src/engine/surface_collision.c",
        "headers": ["host/terrain_state.h"],
        "functions": ["find_wall_collisions_from_list", "f32_find_wall_collision",
                      "find_wall_collisions", "find_ceil_from_list", "find_ceil",
                      "unused_obj_find_floor_height", "find_floor_height_and_data",
                      "find_floor_from_list", "find_floor_height", "unused_find_dynamic_floor",
                      "find_floor", "find_water_level", "find_poison_gas_level",
                      "unused_resolve_floor_or_ceil_collisions"],
    },
    "main_pool.c": {
        "source": "src/game/memory.c",
        "headers": ["host/main_pool.h"],
        "functions": ["main_pool_init", "main_pool_alloc", "main_pool_free",
                      "main_pool_realloc", "main_pool_available", "main_pool_push_state",
                      "main_pool_pop_state", "alloc_only_pool_init", "alloc_only_pool_resize"],
    },
    "math_util.c": {
        "source": "src/engine/math_util.c",
        "omit": ["mtxf_to_mtx", "mtxf_rotate_xy"],
    },
    "object_nodes.c": {
        "source": "src/engine/graph_node.c",
        "headers": ["sm64.h", "engine/graph_node.h", "engine/math_util.h",
                    "engine/geo_layout.h", "game/area.h", "game/memory.h"],
        "functions": ["init_scene_graph_node_links", "init_graph_node_object",
                      "geo_add_child", "geo_remove_child", "geo_make_first_child",
                      "geo_reset_object_node", "geo_obj_init", "geo_obj_init_spawninfo"],
    },
    "animation.c": {
        "source": "src/engine/graph_node.c",
        "headers": ["sm64.h", "engine/graph_node.h", "game/area.h", "game/memory.h"],
        "functions": ["geo_obj_init_animation", "geo_obj_init_animation_accel",
                      "retrieve_animation_index", "geo_update_animation_frame"],
    },
}

def function_range(data, name):
    # Blank comments and literals without changing offsets. Braces in comments
    # and strings must not terminate a function body.
    token = rb'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    masked = re.sub(token, lambda m: bytes(10 if b == 10 else 32 for b in m[0]),
                    data, flags=re.S)
    pattern = rb'^[^\n;{}]*\b' + name.encode() + rb'\s*\([^;{}]*\)\s*\{'
    matches = list(re.finditer(pattern, masked, re.M))
    if len(matches) != 1:
        raise ValueError(f"Expected one definition of {name}, found {len(matches)}")
    start = matches[0].start()
    depth = 1
    end = matches[0].end()
    while depth and end < len(masked):
        depth += (masked[end] == ord('{')) - (masked[end] == ord('}'))
        end += 1
    if depth:
        raise ValueError(f"Unterminated function {name}")
    return start, end

def main():
    checkout = Path(sys.argv[1]).resolve()
    upstream = json.loads((ROOT / "upstream.json").read_text())
    revision = subprocess.check_output(
        ["git", "-C", str(checkout), "rev-parse", "HEAD"], text=True).strip()
    if revision != upstream["revision"]:
        raise ValueError("Checkout does not match the pinned upstream revision")
    outputs = {}
    for output, spec in EXTRACTIONS.items():
        # Read committed bytes, not potentially edited working-tree files.
        data = subprocess.check_output(
            ["git", "-C", str(checkout), "show", f"{revision}:{spec['source']}"])
        contents = b"/* Generated verbatim upstream function extraction. See extracted.json. */\n"
        if "omit" in spec:
            excluded = sorted(function_range(data, name) for name in spec["omit"])
            copied = []
            previous = 0
            contents += f'#line 1 "n64decomp/{spec["source"]}"\n'.encode()
            for start, end in excluded + [(len(data), len(data))]:
                body = data[previous:start]
                contents += body
                copied.append({"start": previous, "end": start,
                               "sha256": hashlib.sha256(body).hexdigest()})
                contents += b"\n" * data[start:end].count(b"\n")
                previous = end
            (ROOT / "extracted").mkdir(exist_ok=True)
            (ROOT / "extracted" / output).write_bytes(contents)
            outputs[output] = {"source": spec["source"],
                               "source_sha256": hashlib.sha256(data).hexdigest(),
                               "sha256": hashlib.sha256(contents).hexdigest(),
                               "copied_ranges": copied, "omitted_functions": spec["omit"],
                               "functions": {}}
            continue
        for header in spec["headers"]:
            if not (ROOT / "upstream" / "include" / header).exists() and not (
                    ROOT / "upstream" / "src" / header).exists() and not (ROOT / header).exists():
                raise ValueError(f"Missing imported header {header}")
            contents += f'#include "{header}"\n'.encode()
        functions = {}
        for name in spec["functions"]:
            start, end = function_range(data, name)
            body = data[start:end]
            line = data[:start].count(b"\n") + 1
            contents += f'\n#line {line} "n64decomp/{spec["source"]}"\n'.encode() + body + b"\n"
            functions[name] = {"start": start, "end": end,
                               "sha256": hashlib.sha256(body).hexdigest()}
        if "footer" in spec:
            contents += f'\n#include "{spec["footer"]}"\n'.encode()
        (ROOT / "extracted").mkdir(exist_ok=True)
        (ROOT / "extracted" / output).write_bytes(contents)
        outputs[output] = {"source": spec["source"],
                           "source_sha256": hashlib.sha256(data).hexdigest(),
                           "sha256": hashlib.sha256(contents).hexdigest(),
                           "functions": functions}
    (ROOT / "extracted.json").write_text(json.dumps({"revision": revision, "files": outputs}, indent=2) + "\n")

if __name__ == "__main__":
    main()
