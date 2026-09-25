# sm64_physics

Super Mario 64 as a native library: the whole game, stepped one frame at a
time, bit-identical to the console. Its code in `game/` started as
[n64decomp/sm64](https://github.com/n64decomp/sm64)'s (at `9921382`) and is the
library's own; the decomp stays a reference. `docs/changes.md` lists what
changed from it.

This is the rewrite. Where it is headed:

- **The whole game, not a physics model.** Level scripts, objects, the camera,
  menus and dialogs run as the decomp wrote them, from power-on, so TAS inputs
  made against the library replay on an emulator and back.
- **No graphics in a game step.** The render walk's drawing is behind a runtime
  switch (`sm64_set_draw`, `platform/draw.h`): a step without it keeps only
  what changes game state (camera, animation frames, geo functions' state).
  Drawing a frame reruns its step with drawing on.
- **No ROM data in the library.** What the decomp extracts from the ROM
  (textures, skyboxes, demo inputs, sound) is loaded from the user's ROM
  (`sm64_load_rom`); `game/rom_assets/` lists where each is.
- **Any number of worlds, on any threads.** Each world is a console of its
  own (`sm64_world_create`); threads step different worlds at the same time.
- **Verified against an emulator.** `oracle/` records the game's state at every
  frame of real TAS movies on mupen64plus; the library must reproduce it.

## Status

JP, US, EU and the Shindou Edition. In lockstep with the emulator (`sm64_lockstep`), comparing Mario,
all objects, the camera and its internal state, cutscene and menu state,
controllers, areas and the save file every frame, these TASes are identical
from power-on to their last frame:

| Movie | Version | Frames |
|---|---|---|
| 1 key (TASVideos 4490M) | JP | 7431 |
| all trees (7239M) | JP | 14609 |
| 0 stars (2016M) | US | 8827 |
| 16 stars (6943M) | US | 23303 |
| 70 stars (2062M) | US | 74451 |
| 120 stars (7310M) | US | 128863 |
| no input: the title demos (`oracle/make_movie.py idle`) | EU | 20000 |
| random input, seed 64 (`oracle/make_movie.py random`) | EU | 30000 |
| no input: the title demos | Shindou | 20000 |
| random input, seed 64 | Shindou | 30000 |

The US 16, 70 and 120 star movies do not play out as published on this
emulator (mupen64plus), which the TASes were not made on: the library follows
the emulator there, desyncs included. EU and the Shindou Edition have no TAS in
the corpus yet; their made-up movies cover the title demos, the menus
(languages included) and play on the castle grounds. The Shindou Edition also
reads the controller while looking for a Rumble Pak, at boot and every 60
vertical interrupts without one: `sm64_boot_polls` counts the boot's, and
the lockstep comparator skips the others.

`docs/avoid_ub.md` lists where a native build of the decomp differs from the
N64 and how each difference is handled.

## Sound

With `sm64_set_audio` on, a step also runs the sound thread, and the RSP's
audio microcode runs its command lists (`platform/rsp_audio.c`, both
microcodes: JP/US/EU's and the Shindou Edition's later one); `sm64_audio`
returns what the game handed the audio interface during the step, stereo
16-bit at the console's DAC rate (about 32 kHz). The sound banks and
sequences are converted from the user's ROM (`platform/sound.c`). The game
reads nothing back from the sound thread, so a step's state is the same
with or without it.

`sm64_oracle --audio OUT` records the emulator's sound the same way. Against
it (mupen64plus's HLE of the microcode), JP, US and EU correlate at 0.998 or
better in the first seconds of a movie, and the Shindou Edition matches sample
for sample except where the game negates one side of a note panned hard to
the other, which that HLE leaves out. After that the streams drift apart in
time: on the console the game's waits for the sound thread cost vertical
interrupts, which a step does not model.

## Building

Needs CMake, a C compiler and Python 3. One library per game version
(`SM64_VERSION`: jp, us, eu or sh).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSM64_VERSION=jp
cmake --build build
./build/sm64_run oracle/out/jp-1key.polls --rom "Super Mario 64 (Japan).z64" --trace build/jp-1key.native.trace
python3 oracle/sm64trace.py diff oracle/out/jp-1key.trace build/jp-1key.native.trace
```

Windows builds use MinGW-w64, as frametee's do; `cmake/mingw-w64.cmake`
cross-builds from Linux, and `sm64_run.exe` runs under Wine (its traces are
the same as the Linux build's):

```sh
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-win
WINEPATH=/usr/x86_64-w64-mingw32/bin wine build-win/sm64_run.exe oracle/out/jp-1key.polls --rom ... --trace ...
```

`-DSM64_PHYSICS_SHARED=ON` builds a shared library instead, as frametee does:
one copy of the game for a program and its plugins, since a world holds
addresses of the library's code. It exports the API (`include/sm64_physics.h`)
and, with protected visibility, the game's own functions and variables for
code that uses the game directly. The tools (`sm64_run`, `sm64_lockstep`) are
only built with the static library. A program that wants several versions
adds this directory once per version, with `SM64_VERSION` and
`SM64_TARGET_SUFFIX` set (frametee: `sm64_physics_us`, `sm64_physics_jp`).

## Layout

| | |
|---|---|
| `platform/host.c` | Power-on and the per-frame loop: what the N64's threads do, without threads. |
| `platform/ultra.c` | The SDK functions the game calls: message queues, DMA as copies, the controller, EEPROM. |
| `platform/ultra_math.h` | libultra's `sinf`/`cosf` for the game, under other names. |
| `game/` | The game's code. `game/gen/<version>/` holds what the decomp generates from its own sources (text, level headers); `game/rom_assets/<version>.tsv` lists what comes from the ROM. |
| `platform/draw.h` | The runtime switch for the render walk's drawing. |
| `platform/world.c`, `world.h`, `state.ld` | Worlds: the game's state as one section, one copy per world (`docs/state.md`). |
| `tools/state/` | The rewrite that routes every use of the state through the current world, and the initial values' pointer table. |
| `tools/vendor.py` | The one-time import from the decomp, kept as a record. |
| `tools/rom_stubs.py` | Names for the ROM's textures, without their pixels, for the build. |
| `tools/n64stack/` | The N64 stack pointer model's table (`<version>.tsv`) and the tools that derive it from a decomp build. |
| `tools/run.c` | Steps an oracle polls file and writes a trace in the oracle's format, laid out as on the N64. |
| `oracle/` | The emulator reference. |

## Build flags

`-fwrapv -ffp-contract=off -fno-fast-math -fsigned-char -fno-strict-aliasing`:
signed integers wrap and chars are signed as with IDO, and single-precision
arithmetic is neither fused nor reordered.

`AVOID_UB` and `NON_MATCHING` are defined because the decomp requires them for
non-N64 builds. `NON_MATCHING` changes nothing (no function is left in
assembly). `AVOID_UB` replaces undefined behavior at about 38 places in game
code with defined results; each site must be checked against what the N64
actually does.
