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
`platform/n64stack.c`: the stack model's table and, per thread, its stack
pointer; the RSP's output buffer).

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

## Saved states

`sm64_save_state` copies a world's memory; the copy holds addresses in that
world's memory, so it loads only into the same world. Moving a state to
another world needs every address in it moved, including the ones in the
memory pools, whose contents have no static type: the next step.
