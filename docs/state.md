# The game's state and worlds

A world is one console: the game's whole state, in memory of its own. Any
number of worlds can exist, and threads can step different worlds at the same
time (`sm64_world_create`, `sm64_step`).

## The state section

Every writable variable of the game, and of the host's stand-ins for the
console (`platform/host.c`, `platform/ultra.c`), is linked into one section,
`sm64_state` (`platform/state.ld`). That covers globals, statics and function
statics, the memory pools (`sPoolMemory`, the display list pools) and the
level data the game writes to. The variables are defined exactly as the
decomp wrote them, initial values included. What is not state stays outside:
constant data, the code, and what the process shares (`platform/world.c`: the
state's initial values, each thread's current world and settings;
`platform/n64stack.c`: per thread, the stack model's pointer; the RSP's
output buffer).

The section is page aligned at both ends. Ranges of it, marked by symbols,
are what the N64 reloads from ROM: `sm64_overlay_*` (src/menu and
src/goddard, reset by `FIXED_LOAD`) and `sm64_level_data_*` (every level's
data, reset by `LOAD_MIO0` of segment 7).

## Worlds

A world is a copy of the section in memory of its own, aligned as the
section is. The code reaches the current world's copy of a variable `x`
through `WORLD(x)` (`platform/world.h`): the variable's address moved by the
thread's current world offset, a thread-local value that `sm64_step` sets. So
all the game's code, macros included, reads and writes `WORLD(gMarioState)`
rather than `gMarioState`.

`tools/state/rewrite.py` makes it so: it parses every source of the state
with libclang (with the build's flags, from `compile_commands.json`) and
rewrites each use of a state variable inside a function body where the name
is spelled, in the function or in the macro it comes from. Initializers of
variables are data, not code, and stay as they are. It has to run for every
version (JP and US), since each compiles different code:

```sh
python3 tools/state/rewrite.py build       # SM64_VERSION=jp
python3 tools/state/rewrite.py build-us    # SM64_VERSION=us
```

It is idempotent: on up-to-date sources it changes nothing.

A new world starts from the state's initial values, as a console at power-on
does, and boots. The initial values hold addresses of other state variables
(`gMarioState = &gMarioStates[0]`, the painting groups); those move to the
new world's memory. The build lists them from the relocations of the
library's object (`tools/state/init_pointers.py`), which also fails the build
if any data outside the state (constant tables, behavior scripts) holds an
address in it: code reading such a pointer would reach the section itself.

Once a world exists, the section itself is made inaccessible
(`mprotect(PROT_NONE)`): code that reaches the state without `WORLD()` faults
at once instead of quietly sharing a variable between worlds.

## Checks

- `sm64_lockstep`: one world against the emulator, every frame.
- `sm64_run POLLS --threads N`: N worlds stepped at the same time on N
  threads; every frame's trace record, and every variable at the end, has to
  be the same in all of them. (The memory pool is left out of the final
  comparison: its freed memory keeps parts of each world's own addresses,
  which nothing reads.)
- `sm64_run POLLS --check-state FRAME`: save, run on, load, run again.

## Copying worlds: the pointer map

The state keeps addresses of itself: objects point at objects, graph nodes at
graph nodes, the pools' headers at each other. Copying a world into another's
memory has to move exactly those, and a value is not enough to tell: an angle
of `0x7xxx` next to a zero looks like the upper half of an address. So every
world keeps a pointer map, one bit per 4 bytes of its memory, saying where an
address may be kept (`platform/pointers.h`). `sm64_world_copy` copies the
memory and the map and moves every marked value that points into the source
world by the distance between the two; values pointing elsewhere (the code,
constant data) stay.

The map comes from types:

- Variables: `tools/state/types.py` describes, with libclang, every state
  variable whose type holds addresses, as `offsetof` expressions so the
  compiler lays them out, into `game/gen/<version>/pointers/<source>.inc.c`;
  each source includes its file at its end, which registers its variables at
  load time. A new world's map starts from them.
- Memory handed out at run time: the allocators (the main pool, alloc-only
  pools, memory pools, goddard's heap, the sound pools) clear the map of
  every block they hand out and mark their own headers; the code that
  allocates marks what it keeps there with `host_mark(p, HOST_TYPE_OF(Type),
  count)`. The descriptors of those types are generated too
  (`game/gen/<version>/pointers/types.c`).

Where the same memory is used by two owners at once, as the title screen's
goddard heap and the surface pools can be, the last one to mark it wins; what
the other keeps there is not read while it is.

`types.py`, like `rewrite.py`, runs for each version after the game's code
changes; its output is committed.

### Checks

- `sm64_run POLLS --pointers N`: two worlds stepped alike at different
  addresses; every N frames, the words where they differ by exactly the
  distance between them are the state's addresses of itself. Those the map
  misses are listed, with the code that allocated them
  (`SM64_NOTE_ALLOCATIONS=1` records it). Addresses left in memory nothing
  allocated holds are counted apart: nothing reads them.
- `sm64_run POLLS --move N` and `SM64_LOCKSTEP_MOVE=N sm64_lockstep`: every N
  frames the world is copied to new memory and the old one overwritten and
  freed; an address the copy missed faults or changes the run.
- `sm64_run POLLS --check-state FRAME`: saves at FRAME, runs on, loads the
  state into another world and runs again.

## Saved states

`sm64_save_state` writes where the world's memory was, the memory and its
map; `sm64_load_state` loads it into any world of the process, moving the
addresses as a copy does. A saved state also holds addresses of the
library's code and constant data, so it is only valid in the process that
saved it.
