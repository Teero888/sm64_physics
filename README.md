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

Upstream's `NON_MATCHING` and `AVOID_UB` host paths are enabled. Floating-point
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
object behavior. Test-only globals and debug hooks live in `tests/collision.c`,
not the production library.

To reproduce the source import from a checkout of the recorded revision:

```sh
python3 tools/import_upstream.py /path/to/n64decomp-sm64
```

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
