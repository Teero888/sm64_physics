# sm64_physics

Work in progress: an independent native SM64 simulation library derived from
[n64decomp/sm64](https://github.com/n64decomp/sm64). FrameTee will own ROM
decoding, asset storage, camera presentation and rendering. This repository
will own Mario movement, interactions, object behavior and collision state.
No ROM is needed to build or run the current tests.

## Current extraction

`upstream.json` pins the upstream revision and records the SHA-256 of every
imported file. `upstream/` contains byte-identical source files and the local
include dependencies needed to compile them. Host adaptation belongs in
`host/`, outside those source files. The upstream license is preserved at
`upstream/LICENSE.md`.

`extracted/` contains selected complete functions copied verbatim from mixed
upstream files. `extracted.json` records the source file hash, original byte
ranges and function hashes. The animation routines are extracted this way
without importing the scene renderer. `host/animation.c` owns the frame/time
mutations previously performed during rendering and calls the original frame
calculation. This hook is tested, but is not yet connected to a world tick.

The object-node link and initialization functions are also extracted unchanged.
They keep the data bookkeeping used by the original object allocator, without
scene traversal or GPU work. The compiled math extraction omits the two N64
fixed-point matrix packing functions; all bytes outside those named functions
are verified against the original. Other mixed presentation functions in the
behavior sources still need separation.

The host allocation-only pool aligns native allocations to `max_align_t` and
rejects requests that exceed its supplied storage. This is a host memory
adapter, not a modification to a decompiled physics algorithm. Object-node
globals are currently single-context state; owned worlds and concurrent
simulation are still pending.

The original main-pool allocation, free, resize and push/pop functions now run
against `sm64_host_main_pool` storage. Their source bodies are extracted
verbatim; a private compatibility header maps their global state names onto a
thread-local active pool. Creation/destruction and storage ownership live in
the host adapter. This isolates pool memory only, not the rest of simulation.
Calls retain the original allocator's preconditions: valid active pool,
bounded internal allocation sizes and valid stack operations. Upstream pop
restores head pointers but leaves sentinel links for the next allocation to
renew; it is not a general-purpose snapshot/restore API.

`host/terrain.h` accepts decoded triangle and environment-region data. Its
owned instances use the original surface normal calculation, cell insertion,
ordering and query functions. Creation copies host data and checks pool limits;
terrain copies rebuild their own surfaces and links. Native queries now require
an activated terrain, whose state is thread-local. This isolates terrain only:
the full area loader has not been routed into this context, and Mario/object
globals remain to be adapted.
Do not use the archive as a full simulation until those paths share one world.

The original moving-object collision loader, vertex transformation and dynamic
partition clearing now use the owned terrain context. A checked host entry
validates the decoded collision stream, the original 200-vertex temporary
buffer limit and remaining pool capacity before invoking the loader. Object
transform helpers are extracted unchanged from their mixed source file. The
loader retains the original time-stop, distance, room and object-flag behavior.
Terrain copies retain current dynamic geometry but borrow object/Mario/behavior
references; a complete world clone must clone and rebind those objects.

Original Mario ground quarter-steps, ground movement, gravity and collision
helpers now run independently of the remaining action dispatcher. The terrain
context owns the shell-water pseudo-floor and level number used by those
helpers. This makes direct native ground movement executable, but does not yet
implement a complete input-driven Mario/world tick. Original air quarter-steps,
air movement, ledge checks and vertical wind now use the same terrain context;
the complete airborne action dispatcher is still pending.

The Mario action groups, movement, interactions, collision, object processing,
behavior interpreter and object behaviors compile as a native static archive.
**The archive is not yet a usable world simulation library.** In particular:

- Host services and asset references remain unresolved until an executable
  supplies them. Building the archive does not prove these are implemented.
- Some original files mix simulation with presentation functions. These must
  be separated at the source boundary before the production library is linked;
  they are not a license to introduce rendering into the physics API.
- World ownership, independent clone/restore and the public host-data API are
  not implemented yet.
- FrameTee still uses its old backend. This repository is not wired to it yet.

Upstream's `NON_MATCHING`, `AVOID_UB` and `NO_SEGMENTED_MEMORY` host paths are
enabled. FrameTee must resolve segmented ROM addresses to native pointers
before passing decoded assets to physics. Floating-point
contraction and strict aliasing optimizations are disabled. The target is the
US game initially; regional support and cross-platform execution are unverified.
Emulator bit parity is not a requirement, but the simulation algorithms must
remain the decompiled algorithms.

## Verification

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j6
ctest --test-dir build --output-on-failure
```

`upstream_fidelity` checks every imported file against the manifest and rejects
missing or additional files. `native_collision` loads decoded triangles through
the original surface helpers and checks floor, ceiling, wall, dynamic platform,
camera filtering, water and gas queries. It also checks independent instances,
terrain copy lifetime and degenerate triangles. The old test-only query globals
have been removed. These tests do not exercise the full area/object loader,
Mario actions or object behavior. On Linux, `native_terrain_threads` runs
concurrent queries and checks that floor-geometry scratch state stays separate.

`native_animation` runs without a renderer and checks forward/reverse playback,
looping, clamping, fractional speeds, frozen animations, duplicate calls in a
tick, tick-counter wrap and animation index lookup. The tests use synthetic
animation metadata and do not validate FrameTee's future ROM animation decoder.

`native_object_nodes` checks the original node links, ordering/removal, spawn
transforms and native matrix math. It also checks host allocation alignment and
failure without changing pool state. It does not run the object behavior loop.

`native_main_pool` interleaves two owned pools and checks contents, exhaustion,
left/right allocation, nested push/pop and resizing. It does not prove full
world independence, concurrent stepping or world snapshot support.

`native_moving_surfaces` exercises decoded object collision, translated platform
movement, dynamic clearing, time stop, distance culling, DDD warp room assignment,
invalid stream rejection and dynamic-geometry copy lifetime. It does not run
the behavior interpreter or Mario stepping.

`native_mario_ground` executes the original ground step against owned terrain,
checking displacement, wall response and shell-water support. It also checks
normal gravity, jump-height release, terminal velocity, long-jump gravity and
twirl gravity. It sets Mario state directly; it does not verify controller input,
action transitions or complete level gameplay.

`native_mario_air` executes the original air step and checks falling, landing,
ceiling response, ceiling/ledge grabs and lava-wall collision. These are direct
movement tests, not complete airborne action or level-transition tests.

To reproduce the source import from a checkout of the recorded revision:

```sh
python3 tools/import_upstream.py /path/to/n64decomp-sm64
python3 tools/extract_upstream.py /path/to/n64decomp-sm64
python3 tools/verify_upstream.py /path/to/n64decomp-sm64
```

The optional checkout argument to the verifier compares extracted function
bytes directly against the pinned Git commit, in addition to the local hashes.

## Remaining implementation

1. Separate mixed presentation functions while retaining verbatim simulation
   code and recording source provenance for any extraction.
2. Implement native host services and owned world state, including animation
   timing that influences physics, object transforms, save state and events.
   Do not silently stub gameplay dependencies to make linking succeed.
3. Accept decoded collision, behavior, spawn and animation data from the host;
   keep ROM I/O, texture decoding and GPU resources outside this library.
4. Exercise movement, jumping, slopes, water, interactions, moving objects,
   transitions and independent world continuation with executable tests.
5. Integrate FrameTee's independent scene renderer and world adapter, then
   remove sm64ex and its encrypted full-game runtime mechanism.

The configured remote is `https://github.com/Teero888/sm64_physics.git`.
Development is local; the user has explicitly prohibited pushing for now.
