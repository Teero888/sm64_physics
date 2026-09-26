# SM64 oracle

The reference the native library is checked against: TAS movies played on
mupen64plus, with the game's state recorded at every controller poll. A poll
is the start of a game frame, so a trace record holds the state the previous
frame left behind, and the polls file holds the input each frame read — what
the library's `step()` is fed to reproduce the run.

This is a development tool. It is built on its own and never ships.

## Setup

- mupen64plus (core, headers and `mupen64plus-rsp-hle`), e.g. `pacman -S mupen64plus`
- The ROMs in FrameTee's `data/games/sm64/`: `Super Mario 64 (Japan).z64`
  (SHA-1 `8a20a5c8…`) and `Super Mario 64 (USA).z64` (`9bef1128…`)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
python3 corpus.py            # downloads, runs and checks every movie in corpus.json
```

## Tools

| | |
|---|---|
| `build/sm64_oracle` | Plays a movie and writes a trace (`--trace`), the polls file (`--polls`) and RDRAM dumps (`--dump-at N`), and with `--audio OUT` the sound the game hands the audio interface (stereo 16-bit samples). `--poll-offset` (default -1, see Findings) says which movie sample a poll reads. `--poke POLL:ADDRESS:VALUE` writes a word before the frame after POLL, to find when the game last wrote it. `--break ADDRESS` and `--watch ADDRESS[:SIZE]` (a physical address) log each call with its stack pointer and each store with its value, from `--debug-from POLL` on; they need `--cpu 1` and a core built with the debugger (mupen64plus-core 2.6.0: `CFLAGS="-I/usr/include/minizip -DFALSE=0 -DTRUE=1" make -C projects/unix all DEBUGGER=1 NO_ASM=1`, then `--core` it). |
| `corpus.py` | Runs `corpus.json` and checks each movie's level route and final action. |
| `make_movie.py` | Movies for a version without a TAS: no input (the title demos) or seeded random input. |
| `sm64trace.py` | `info`, `show`, `mario` (decoded Mario state) and `diff` (first differing poll and field) for traces. |
| `levels.py` | The level sequence of a trace. |
| `symbols.py` | Builds `symbols/*.tsv` from a decomp build, including static variables. |
| `fields/*.txt` | Which symbols a trace records. `hash=N` stores one CRC-32 per N bytes (the object pool is hashed per object). |

## Symbols

`symbols/jp.tsv` and `symbols/us.tsv` come from matching builds of
[n64decomp/sm64](https://github.com/n64decomp/sm64) at `9921382`. IDO leaves
static variables out of the ELF's symbol table, so `symbols.py` reads them from
each object's `.mdebug` data and places them with the linker map. A static
name used by several files is written `file.c:name`.

To regenerate (needs `mips64-elf-binutils` and `python-capstone`):

```sh
git clone https://github.com/n64decomp/sm64 ~/software/sm64-decomp && cd ~/software/sm64-decomp
cp ".../Super Mario 64 (Japan).z64" baserom.jp.z64
# armips does not build with GCC 16 as configured:
make -C tools armips_CFLAGS="-std=gnu++11 -fno-exceptions -fno-rtti -pipe -include cstdint"
# GNU make 4.4 links leveldata.elf with the generic %.elf rule, which lacks the
# texture symbols; in the Makefile's generic rule add
#   $(if $(TEXTURE_BIN),--just-symbols=$(BUILD_DIR)/bin/$(TEXTURE_BIN).elf)
make VERSION=jp -j8 || make VERSION=jp -j8   # the second run covers a link-order race
sha1sum -c sm64.jp.sha1
python3 .../oracle/symbols.py build/jp sm64.jp .../oracle/symbols/jp.tsv
```

## Findings

- **Movies must be per poll (.m64).** A BizHawk .bk2 holds one input per
  vertical interrupt, so it only replays on an emulator whose timing puts every
  poll in the same interrupt as BizHawk's did. mupen64plus 2.6 does not, and
  the published .bk2 desyncs. SM64 TASes are made on Mupen64-rr as .m64 and
  TASVideos keeps the originals as additional files.
- **Movie sample N is the console's controller read N + 1.** Mupen64-rr,
  which the TASes are made on, gives the first read after power-on no movie
  sample (`--poll-offset -1`, the default). Found with Mupen64-rr 1.4.0-5
  itself under Wine, logging the game every poll from Lua: with it, the 16-star
  TAS is identical on both emulators at every poll, and the 16, 70 and 120-star
  TASes play to their end on mupen64plus. Earlier, per-movie offsets (-16, -55)
  were found by trying: they put the first Start press where the title screen
  takes it, but misplace every input after, which the long runs do not survive.
- **Game state does not depend on emulated timing.** The dynarec and the
  cached interpreter give identical traces over the whole 1-key TAS, although
  RAM differs (the audio heap, around `0x801ce000`–`0x801e0000`, and some audio
  globals): the audio thread runs at different moments, game logic does not.
- **The dummy video plugin must raise the DP interrupt** at the end of each
  display list, as real plugins do on the full sync; without it the game waits
  forever after boot.
- mupen64plus 2.5+ randomizes interrupt timing unless `RandomizeInterrupt` is
  off; the oracle turns it off and runs are byte-identical.
