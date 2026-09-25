#!/usr/bin/env python3
"""Read and compare oracle traces.

  sm64trace.py info TRACE                 fields and record count
  sm64trace.py show TRACE POLL [FIELD]    one record's fields, in hex
  sm64trace.py diff EXPECTED ACTUAL       first poll and fields that differ
  sm64trace.py mario TRACE POLL [COUNT]   Mario's decoded state over COUNT polls
"""
import struct
import sys


class Trace:
    def __init__(self, path):
        data = open(path, "rb").read()
        if data[:8] != b"SM64ORC1":
            sys.exit(f"{path}: not an oracle trace")
        (count,) = struct.unpack_from("<I", data, 8)
        at = 12
        self.fields = []
        record = 12
        for _ in range(count):
            (length,) = struct.unpack_from("<H", data, at)
            name = data[at + 2 : at + 2 + length].decode()
            address, size, stride = struct.unpack_from("<III", data, at + 2 + length)
            at += 2 + length + 12
            stored = size // stride * 4 if stride else size
            self.fields.append((name, address, size, stride, record, stored))
            record += stored
        self.record_size = record
        self.body = memoryview(data)[at:]
        self.count = len(self.body) // record

    def record(self, index):
        return self.body[index * self.record_size : (index + 1) * self.record_size]

    def header(self, index):
        return struct.unpack_from("<III", self.record(index), 0)

    def field(self, index, name):
        for field in self.fields:
            if field[0] == name:
                offset, stored = field[4], field[5]
                return field, bytes(self.record(index)[offset : offset + stored])
        sys.exit(f"no field {name}")


# Pointer members, (offset, length) in the N64 layout. They hold addresses,
# which only mean something within one run, so diffs ignore them.
POINTERS = {
    "gMarioStates": [(0x60, 0x0C), (0x78, 0x2C)],
}


def masked(name, data):
    for offset, length in POINTERS.get(name, []):
        data = data[:offset] + bytes(length) + data[offset + length:]
    return data


def hexdump(data, base=0, width=16):
    return "\n".join(f"  +{base + i:05x}: {data[i:i + width].hex(' ')}" for i in range(0, len(data), width))


def diff_bytes(name, stride, expected, actual, limit=24):
    lines = []
    unit = 4 if stride else 1
    for i in range(0, len(expected), unit):
        if expected[i : i + unit] != actual[i : i + unit]:
            where = f"chunk {i // 4} (+0x{i // 4 * stride:x})" if stride else f"+0x{i:x}"
            lines.append(f"    {name} {where}: {expected[i:i + unit].hex()} -> {actual[i:i + unit].hex()}")
            if len(lines) == limit:
                lines.append("    ...")
                break
    return lines


def describe_mario(trace, index):
    """One line of gMarioStates (JP/US layout) and the level around it."""
    _, mario = trace.field(index, "gMarioStates")
    action, = struct.unpack_from(">I", mario, 0x0C)
    action_state, action_timer = struct.unpack_from(">HH", mario, 0x18)
    face_yaw, = struct.unpack_from(">h", mario, 0x2E)
    pos = struct.unpack_from(">fff", mario, 0x3C)
    vel = struct.unpack_from(">fff", mario, 0x48)
    forward, = struct.unpack_from(">f", mario, 0x54)
    coins, stars, keys, lives, health = struct.unpack_from(">hhbbh", mario, 0xA8)
    level = struct.unpack(">h", trace.field(index, "gCurrLevelNum")[1])[0]
    area = struct.unpack(">h", trace.field(index, "gCurrAreaIndex")[1])[0]
    timer = struct.unpack(">I", trace.field(index, "gGlobalTimer")[1])[0]
    seed = struct.unpack(">H", trace.field(index, "gRandomSeed16")[1])[0]
    poll, vi, value = trace.header(index)
    return (f"poll {poll:5} vi {vi:5} in {value:08x} timer {timer:5} rng {seed:5} level {level:2}.{area} "
            f"act {action:08x}/{action_state}/{action_timer:<3} pos ({pos[0]:9.2f} {pos[1]:9.2f} {pos[2]:9.2f}) "
            f"fwd {forward:7.2f} yaw {face_yaw:6} hp {health:#06x} stars {stars} keys {keys} coins {coins} lives {lives}")


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    command, trace = sys.argv[1], Trace(sys.argv[2])
    if command == "info":
        print(f"{trace.count} records of {trace.record_size} bytes")
        for name, address, size, stride, _, _ in trace.fields:
            print(f"  {name:32} 0x{address:08x} {size:6}" + (f" hashed per {stride}" if stride else ""))
    elif command == "show":
        index = int(sys.argv[3], 0)
        poll, vi, value = trace.header(index)
        print(f"poll {poll}, vertical interrupt {vi}, input {value:08x}")
        for field in trace.fields:
            if len(sys.argv) > 4 and field[0] != sys.argv[4]:
                continue
            _, data = trace.field(index, field[0])
            print(f"{field[0]}:\n{hexdump(data)}")
    elif command == "diff":
        other = Trace(sys.argv[3])
        names = [f[0] for f in trace.fields]
        shared = [f for f in other.fields if f[0] in names]
        # Records line up by poll number: a native trace starts at the first
        # game frame, after the oracle's boot-time poll.
        by_poll = {trace.header(i)[0]: i for i in range(trace.count)}
        compared = 0
        for index in range(other.count):
            poll = other.header(index)[0]
            if poll not in by_poll:
                continue
            mine = by_poll[poll]
            if trace.header(mine)[2] != other.header(index)[2]:
                sys.exit(f"inputs differ at poll {poll}: the traces are not of the same run")
            bad = []
            for field in shared:
                expected = masked(field[0], trace.field(mine, field[0])[1])
                actual = masked(field[0], other.field(index, field[0])[1])
                if expected != actual:
                    bad += [f"  {field[0]}:"] + diff_bytes(field[0], field[3], expected, actual)
            if bad:
                print(f"first difference at poll {poll} (vertical interrupt {trace.header(mine)[1]}), "
                      f"after {compared} identical polls:")
                print("\n".join(bad))
                sys.exit(1)
            compared += 1
        print(f"identical over {compared} polls ({', '.join(f[0] for f in shared)})")
    elif command == "mario":
        start = int(sys.argv[3], 0)
        if start < 0:
            start += trace.count
        for index in range(start, min(trace.count, start + (int(sys.argv[4], 0) if len(sys.argv) > 4 else 1))):
            print(describe_mario(trace, index))
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
