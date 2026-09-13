#!/usr/bin/env python3
"""Import byte-identical simulation sources and their local include closure."""
import hashlib
import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCES = [
    "src/game/sound_init.c", "src/game/spawn_sound.c", "src/game/memory.c",
    "src/game/game_init.c", "src/engine/graph_node.c",
    "src/engine/math_util.c", "src/engine/surface_collision.c",
    "src/engine/surface_load.c", "src/game/mario.c", "src/game/mario_step.c",
    "src/game/interaction.c", "src/engine/behavior_script.c",
    "src/game/object_collision.c", "src/game/object_helpers.c",
    "src/game/object_list_processor.c", "src/game/platform_displacement.c",
    "src/game/spawn_object.c", "src/game/macro_special_objects.c",
    "src/game/obj_behaviors.c", "src/game/obj_behaviors_2.c",
    "src/game/behavior_actions.c", "data/behavior_data.c",
] + [f"src/game/mario_actions_{name}.c" for name in (
    "airborne", "automatic", "cutscene", "moving", "object", "stationary", "submerged"
)]

def main():
    upstream = pathlib.Path(sys.argv[1]).resolve()
    revision = subprocess.check_output(
        ["git", "-C", str(upstream), "rev-parse", "HEAD"], text=True).strip()
    pending = [upstream / name for name in SOURCES + ["LICENSE.md"]]
    files = {}
    while pending:
        path = pending.pop()
        name = path.relative_to(upstream).as_posix()
        if name in files:
            continue
        data = path.read_bytes()
        files[name] = hashlib.sha256(data).hexdigest()
        output = ROOT / "upstream" / name
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(data)
        for include in re.findall(rb'^\s*#\s*include\s*[<"]([^>"\n]+)[>"]', data, re.M):
            include = include.decode()
            for base in (path.parent, upstream / "include", upstream / "src", upstream):
                candidate = (base / include).resolve()
                if candidate.is_file() and candidate.is_relative_to(upstream):
                    pending.append(candidate)
                    break
    (ROOT / "upstream.json").write_text(json.dumps({
        "repository": "https://github.com/n64decomp/sm64.git",
        "revision": revision, "sources": SOURCES, "files": dict(sorted(files.items()))
    }, indent=2) + "\n")

if __name__ == "__main__":
    main()
