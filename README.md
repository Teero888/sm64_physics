# sm64_physics

Super Mario 64 as a native library: the game code of
[n64decomp/sm64](https://github.com/n64decomp/sm64), compiled for the host and
stepped one game frame at a time, bit-identical to the console.

This is the rewrite. Where it is headed:

- **The whole game, not a physics model.** Level scripts, objects, the camera,
  menus and dialogs run as the decomp wrote them, from power-on, so TAS inputs
  made against the library replay on an emulator and back.
- **No graphics in the library.** Drawing moves out to FrameTee's SM64 module;
  the parts of the rendering path that change game state (camera update,
  animation frames, dialog and menu state) stay here. Until that split lands the
  build runs the original rendering path into a discarded display list: the
  reference the split is checked against.
- **Verified against an emulator.** `oracle/` records the game's state at every
  frame of real TAS movies on mupen64plus; the library must reproduce it.

## Status

JP only. The JP 1-key TAS matches the oracle over all 7431 frames on every
field compared so far (global timer, RNG, level/area/course/act, Mario's state
without pointers, HUD, dialog).

## Building

Needs a checkout of the decomp at `9921382`, built once for JP from the user's
ROM: that build generates the assets the game data includes (animations, demo
inputs, text, textures, sound). See `oracle/README.md` for building it.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSM64_DECOMP_DIR=~/software/sm64-decomp
cmake --build build
./build/sm64_run oracle/out/jp-1key.polls --trace build/jp-1key.native.trace
python3 oracle/sm64trace.py diff oracle/out/jp-1key.trace build/jp-1key.native.trace
```

## Layout

| | |
|---|---|
| `platform/host.c` | Power-on and the per-frame loop: what the N64's threads do, without threads. |
| `platform/ultra.c` | The SDK functions the game calls: message queues, DMA as copies, the controller, EEPROM. |
| `platform/ultra_math.h` | libultra's `sinf`/`cosf` for the game, under other names. |
| `patches/` | Every change to the decomp, one reviewed patch each, applied to copies in the build tree. |
| `tools/overlay.py` | Applies `patches/`. |
| `tools/native_sound.py` | The sound banks in the host's layout (the decomp's N64 build lays them out big endian, 32-bit). |
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
