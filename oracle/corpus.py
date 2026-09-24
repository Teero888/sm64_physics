#!/usr/bin/env python3
"""Run every movie in corpus.json through the oracle and check that it syncs.

  corpus.py [--roms DIR] [NAME...]

Downloads missing movies into movies/, writes out/NAME.trace and
out/NAME.polls (the input each game frame read: what the native library is
stepped with), and checks the level route and Mario's final action.
ROMs are looked up in --roms (default: FrameTee's data/games/sm64) by region.
"""
import io
import json
import struct
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

from sm64trace import Trace

HERE = Path(__file__).resolve().parent
ROM_NAMES = {"jp": "Super Mario 64 (Japan).z64", "us": "Super Mario 64 (USA).z64"}


def fetch(movie):
    folder = HERE / "movies" / movie["name"]
    found = sorted(folder.glob("*.m64"))
    if found:
        return found[0]
    folder.mkdir(parents=True, exist_ok=True)
    request = urllib.request.Request(movie["url"], headers={"User-Agent": "sm64-oracle"})
    data = urllib.request.urlopen(request, timeout=60).read()
    if data[:4] == b"M64\x1a":
        (folder / f"{movie['name']}.m64").write_bytes(data)
    else:
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            for name in archive.namelist():
                if name.lower().endswith(".m64"):
                    (folder / Path(name).name).write_bytes(archive.read(name))
    found = sorted(folder.glob("*.m64"))
    if not found:
        sys.exit(f"{movie['name']}: the download holds no .m64")
    return found[0]


def route_of(trace):
    route = []
    for index in range(trace.count):
        level = struct.unpack(">h", trace.field(index, "gCurrLevelNum")[1])[0]
        if not route or route[-1] != level:
            route.append(level)
    return route


def main():
    args = sys.argv[1:]
    roms = HERE.parents[2] / "data" / "games" / "sm64"
    if args[:1] == ["--roms"]:
        roms, args = Path(args[1]), args[2:]
    corpus = json.loads((HERE / "corpus.json").read_text())["movies"]
    selected = [m for m in corpus if not args or m["name"] in args]
    (HERE / "out").mkdir(exist_ok=True)
    failures = 0
    for movie in selected:
        path = fetch(movie)
        trace_path, polls_path = HERE / "out" / f"{movie['name']}.trace", HERE / "out" / f"{movie['name']}.polls"
        subprocess.run([str(HERE / "build" / "sm64_oracle"), "--rom", str(roms / ROM_NAMES[movie["rom"]]),
                        "--movie", str(path), "--poll-offset", str(movie["poll_offset"]),
                        "--symbols", str(HERE / "symbols" / f"{movie['rom']}.tsv"),
                        "--fields", str(HERE / "fields" / "core.txt"),
                        "--trace", str(trace_path), "--polls", str(polls_path)],
                       check=True, capture_output=True)
        trace = Trace(trace_path)
        route = route_of(trace)
        end_action = struct.unpack_from(">I", trace.field(trace.count - 1, "gMarioStates")[1], 0x0C)[0]
        ok = route == movie["route"] and f"{end_action:08x}" == movie["end_action"]
        failures += not ok
        print(f"{'ok  ' if ok else 'FAIL'} {movie['name']}: {trace.count} frames, route {route}, "
              f"final action {end_action:08x}")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
