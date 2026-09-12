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
missing or additional files. `native_collision` executes original floor,
ceiling, dynamic platform, camera filtering, water and gas queries against
explicit test state. It does not exercise terrain loading, Mario actions or
object behavior. Test-only globals and debug hooks live in `tests/collision_state.c`,
not the production library.

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
