#!/usr/bin/env python3
"""Print the level sequence of an oracle trace: level@poll for each change."""
import struct
import sys

from sm64trace import Trace, describe_mario

trace = Trace(sys.argv[1])
seen = []
for index in range(trace.count):
    level = struct.unpack(">h", trace.field(index, "gCurrLevelNum")[1])[0]
    if not seen or seen[-1][0] != level:
        seen.append((level, index))
print(" ".join(f"{level}@{index}" for level, index in seen))
if len(sys.argv) > 2:
    print(describe_mario(trace, trace.count - 1))
