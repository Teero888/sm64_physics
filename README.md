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

The host controller adapter accepts raw buttons and signed stick coordinates.
It computes button edges using the original read-controller expression and
calls the verbatim analog adjustment routine. Mario's original button and
joystick processing is extracted separately. `native_controller` covers the
dead zone, stick normalization, button edges, input timers, camera-relative
direction, squish suppression and independent controller histories. Full-world
execution still needs action dispatch and level transitions;
these component tests do not establish a complete playable world.

Original geometry input checks and `update_mario_inputs` now run against the
terrain context, including camera movement flags. `native_mario_geometry_input`
checks water/gas flags, slopes, crushing, OOB position recovery, death-warp
requests, input resets, timers and first-person eligibility. It uses explicit
test observers for the debug callback and warp dependency. No production warp
stub was added: actual transitions and the action dispatcher remain required.

Original `set_mario_action` and its moving/airborne/submerged/cutscene setup
helpers now link independently. `native_mario_action_setup` checks initial jump
velocity and flight to landing through `perform_air_step`, timer resets,
backwards long-jump speed, squish downgrades and surface-dependent walking
setup. This verifies action initialization, not the per-frame action handlers
or the complete `execute_mario_action` dispatcher.

`host/animation_bank.h` accepts decoded animation arrays and owns copies of
their values and indices. Original animation loading, selection, relocation,
frame queries and root translation run against native memory, with no ROM I/O.
Each animated Mario needs its own bank because the original loader reuses one
destination buffer. The bank must outlive its handler and animation pointers.
`native_animation_bank` checks switching, repeated selection, translation,
fractional timing, invalid indices in asset data and independent banks. Full
world cloning still needs to reconstruct and rebind these pointers.

`host/audio.h` exposes synchronous audio request events for the host to consume.
Original cap/shell music bookkeeping, Mario sound flags and particle requests,
and object animation sound triggers are extracted verbatim. Their low-level
audio calls emit typed events; no audio engine or N64 audio thread is linked.
The music state is caller-owned, supports independent activation and cloning,
and permits an explicitly silent sink. Events copy the source coordinates;
source identities are native address cookies and must not be serialized.
`native_audio_events` covers music replacement/fades, duplicate suppression,
independent continuation, animation triggers, Mario flags and landing particles.
The complete world must still own `gAudioRandom` and the object globals, provide
camera-relative source coordinates, and connect FrameTee's event consumer.
The original sound-spawner routine is retained, but its object creation path
still depends on the unfinished full object runtime.

`host/save.h` owns the original US save progression logic and its working and
committed buffers. FrameTee supplies/receives native decoded `SaveBuffer` data;
raw EEPROM byte decoding and file I/O belong to the host. Original signature
checks, backup repair, reload, star/key collection, coin scores, cannon bits,
cap recovery and warp checkpoint functions operate on this context. Cloning
preserves pending changes, committed data and checkpoint state independently.
Sound-mode changes emit an audio event and therefore require an active audio
context as well. `native_save_progress` exercises those gameplay paths,
including the original coin-score truncation behavior and corrupted-backup
recovery. The full world adapter must still synchronize its level/course/act
context with this component and bind other users of the original save globals.

The original `mem_pool_init`, `mem_pool_alloc` and `mem_pool_free` functions
now run on the owned main pool. These provide the auxiliary object storage used
by Chain Chomp and Wiggler. Native bookkeeping and the alignment macro use
`max_align_t` so split block headers remain aligned on x86-64; allocation,
free-list traversal and coalescing bodies remain verbatim. Pools share their
main-pool owner's lifetime and retain the original valid-size/free preconditions.
`native_memory_pool` checks odd-size alignment, fragmented frees, exhaustion,
whole-block reuse, coalescing and independent owners. This does not yet test
complete Chain Chomp or Wiggler behavior execution.

`host/objects.h` owns object slots, free and active lists, and their bookkeeping
parent node. The original allocator, deletion, spawn initialization and
unimportant-object selection operate on this context. The host spawn entry
checks list bounds and exhausted pools before calling upstream; internal
`create_object` retains its original exhaustion semantics. Behavior bytecode
is borrowed. `native_object_lifetime` exercises slot reuse, full-pool eviction,
terrain floor snapping and independent owners. Other object-processing and
behavior functions still use globals that need to be bound to this context;
this is not yet a complete object update loop or cloneable world.

The original `detect_object_collisions` pass now uses those owned object lists
and the context's borrowed Mario reference. `native_object_collision` checks
hitbox boundaries, intangible timers, hurtbox invincibility flags, list-order
priority, the four-object collision limit, destructive-object distance/flag
gates and independent contexts. Interaction dispatch and the complete behavior
update loop still need to be connected; collision records alone do not execute
coin collection, damage or other gameplay interactions.

Original platform tracking and displacement now use the owned object context,
with borrowed Mario/current-object state and an explicit time-stop value.
`native_platform_displacement` checks the four-unit attachment threshold,
translation, quarter-turn rotation for Mario and other objects, time-stop
suppression and independent platform references. Terrain supplies the platform
surface association. The full world adapter still needs to schedule these
functions in the original frame order and synchronize time-stop state across
terrain and objects.

The original behavior interpreter, command table and command handlers are now
extracted with context bindings for the current object/command, frame counter,
model table and random seed. Native bytecode still requires trusted resolved
pointers. The original random generator runs independently in each object
context; `native_random_state` verifies the special seed reset, replay from a
saved seed, independent owners and output ranges.

`host/behavior.h` runs one original `cur_obj_update` using native bytecode,
borrowed model nodes and host-supplied special behavior identities. The original
room, spawning, transform and movement helpers are bound to the object context.
`native_behavior_execution` executes calls/returns, delays, loops, a native
callback, child spawning, movement flags, action timer resets and independent
contexts. It uses test-authored bytecode and a callback observer, not a complete
level's behavior asset set. Game-specific native callbacks still need
full-world integration.

`host/scheduler.h` exposes the original terrain/non-terrain object-list update
phases and deactivated-object cleanup, now bound to the owned object context.
`native_object_scheduler` checks phase order, time-stop exceptions and full
freeze, animation suppression for frozen objects, and 16/32-bit respawn flags
including persistent objects. These phases still require the full frame path
to place terrain clearing, platform displacement and object collisions between
them and to apply deferred time-stop activation.

`host/object_frame.h` now invokes the entire original `update_objects` function,
with live terrain bindings for the current object, Mario and time-stop flags.
The original order of surface clearing/rebuilding, platform displacement,
collision detection, remaining behavior updates, deletion and deferred time
stop is retained. Profiling uses host time; the N64 debug overlay is omitted
while simulation diagnostic counters are reset. `native_object_frame` combines
a moving collision platform, collision observation and deferred time stop.
Its player callback is a test observer: this does not yet execute Mario's full
action dispatcher or provide a complete world
clone. ROM decoding, game-specific callback integration and FrameTee's renderer
are still outstanding.

`host/mario_status.h` exposes original health, cap timing, hitbox and body/camera
output updates using owned frame/debug state. `native_mario_status` checks
poison and water health changes, healing/hurting counters, cap expiry and dialog
timer pauses, short hitboxes, invincibility visibility and camera output.
These routines remain to be invoked through the complete Mario dispatcher;
passing their component test does not establish playable Mario simulation.

Original shared action transitions and held-object interactions now execute
with carry-script identities and Hoot release timing bound to the owned object
context. Hosts supply distinct resolved carry scripts through
`sm64_objects_bind_carry_assets`; non-holdable objects switch to those scripts
when grabbed, dropped or thrown. `native_mario_transitions` checks held-state
changes, original drop/throw placement, script replacement, ride release,
input priority, triple jumps and quicksand overrides. Camera-dependent water
transitions and the complete action dispatcher remain unconnected. The test
uses the owning object context's animation clock.

The 16-bit area animation counter is now owned by each object context. The
full object frame increments it before behaviors (as upstream
`area_update_objects` does), then advances animation state for live objects
with active graph nodes. Original scheduler animation flags preserve time-stop
freezing. The renderer must consume these frames without advancing them.
Standalone animation calls require an active owning context and can set its
tick through `host/animation.h`. Tests cover independent clocks, wraparound,
duplicate calls and frame-level freeze/resume. Selecting/loading active areas,
held-object scene traversal and complete world snapshots remain outstanding.

Original stationary ground movement, moving-sand and horizontal-wind forces,
floor snapping, wall reflection and Bully speed transfer are independently
linked, with wind gust timing bound to the owned frame counter.
`native_mario_environment` exercises stationary downwarping, sand force speed
and direction, wind movement, velocity reset, bonking and collision speed
transfer. Quicksand sinking/death still depends on the remaining Mario
sound/camera transition boundary; full action dispatch is still outstanding.

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
