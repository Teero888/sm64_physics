# Changes to the decomp's code

`game/` started as n64decomp/sm64's code (tools/vendor.py). These are the changes
made to it for the native build, each needed for parity with the N64 or to run
it without the console; they were reviewed patches before the import. Code
refers to them by number, e.g. "(docs/changes.md 10)". `docs/avoid_ub.md` has
the reasoning for the AVOID_UB sites and other native differences.

## 1. Include only ultra64.h in the audio loader

PR/os.h is the SDK's old all-in-one header. Next to ultra64.h it redeclares the
DMA functions and osVirtualToPhysical with 32-bit address types, which conflict
with PR/os_misc.h on a 64-bit host. ultra64.h already declares everything
load.c uses. No behavior change.

Files: `src/audio/load.c`

## 2. Run the sound thread while the game waits for it

On the N64 the game thread spins in wait_for_audio_frames until the
higher-priority sound thread, woken by the next vertical interrupt, has made
an audio frame. Without threads nothing would ever make one; the host runs the
sound thread's work in the loop instead. No change for TARGET_N64.

Files: `src/audio/heap.c`

## 3. Keep the N64's camera union aliasing on 64-bit hosts

GraphNodeCamera.config is a union of a struct Camera pointer and an s32 mode.
Cameras whose geo layout gives no function (the intro's and the menus') only
ever get the mode written, and level_cmd_begin_area then reads the pointer:
on the N64 it is the mode's 32 bits, NULL for these cameras. With 64-bit
pointers the upper half was uninitialized pool memory and the pointer came
out non-NULL. Found by the lockstep comparator (gAreaData[1].camera, file
select). No change for 32-bit builds.

Files: `src/engine/graph_node.c`

## 4. Return the N64's v0 from the overlap tests that miss

detect_object_hitbox_overlap and detect_object_hurtbox_overlap have no return
statement for the no-overlap case. AVOID_UB makes them return 0; the N64
returns whatever v0 held, which is the previous value either of them returned
in the same collision pass. After the first real overlap of a pass every miss
counts as a hit, so the hurtbox test runs for objects Mario is not touching
and leaves INT_SUBTYPE_DELAY_INVINCIBILITY set on them. Mirror v0 instead.
Found by the lockstep comparator (bullies and amps in Bowser in the Fire Sea,
JP 1-key TAS). The disassembly of the JP build shows which instructions write
v0; see the comment in the patch.

EU and the Shindou Edition, built with optimization, leave other values in v0: a collision pass
starts with a list head's address from clear_object_collision, and the
hurtbox test's miss returns &gMarioObject. Both are nonzero. Found by the
lockstep comparator (small breakable boxes, EU title demos).

Files: `src/game/object_collision.c`

## 5. Keep Wiggler's first-frame health at 0 on JP and US

AVOID_UB sets Wiggler's health to 4 in his init, as EU does, because his
first frame of acceleration reads it before anything sets it. Only EU sets
it: on JP, US and the Shindou Edition his health is still the 2048 every
object spawns with, and the speed read with it is out of bounds (25). Found
auditing AVOID_UB (docs/avoid_ub.md).

Files: `src/game/behaviors/wiggler.inc.c`

## 6. Reset the Goddard and menu segment's variables on FIXED_LOAD

On the N64 the title screen, file select and act select code (src/menu) and
the Mario head (src/goddard) are one segment, loaded from ROM by FIXED_LOAD
each time the intro or menu level script runs: all of its variables go back
to their initial values and its bss to zero. Natively they kept their values
from the previous visit. The host now restores them at the same command.

The decomp worked around one symptom for NO_SEGMENTED_MEMORY builds by
calling lvl_init_act_selector_values_and_stars 16 frames early; with the
reset in place the menu script keeps the N64's order.

Files: `levels/menu/script.c`, `src/engine/level_script.c`

## 7. Return the N64's stack address from the water camera transition

Entering the water surface camera mode stores nop_update_water_camera's
result into sAreaYaw. The function has no return statement; the N64 returns
v0, which set_camera_mode last set through vec3f_copy, whose `return &dest`
is the address of its argument slot on the stack: set_camera_mode's stack
pointer. AVOID_UB returned 0. The value depends on the call path (0x6e78 and
0x6eb0 in the JP 1-key TAS), so it comes from the host's model of the N64
stack pointer (docs/avoid_ub.md): every function on the N64's paths to
set_camera_mode opens with `N64_STACK_FRAME(name);`, put there by
`tools/n64stack/frames.py apply`.

Files: `src/game/camera.c`, and those functions' sources (`src/engine`,
`src/game`, `src/menu`)

## 8. Reload a level's data when its script loads segment 7

The level scripts load each level's data (levels/*/leveldata.c) into segment
7 with LOAD_MIO0, decompressing it from ROM on every level load, so whatever
the game wrote into it is gone the next time the level loads. The decomp
already copies the two biggest cases for NO_SEGMENTED_MEMORY builds on every
load (terrain data and macro objects, where collected coins and killed
enemies are marked). What stays is linked in and keeps its values natively:
the paintings (struct Painting, whose floor tracking detects Mario entering
them) and the moving textures' vertices. The host now restores all of the
levels' writable data at the same command.

Files: `src/engine/level_script.c`

## 9. Convert float angles for sins/coss as IDO did

sins and coss index their tables with (u16)(x) >> 4, and are called with
float expressions too (e.g. the Piranha Plant's bubble, coss(animFrame /
doneShrinkingFrame * 0x4000) with animFrame -1). Converting a negative float
to an unsigned integer is undefined in C: x86 wraps it, IDO's code makes it
0xFFFFFFFF. Found by the lockstep comparator (US 16 and 70 stars TASes, the
bubble's scale). platform/ido.h models IDO's conversion.

Files: `src/engine/math_util.h`

## 10. Walk the scene graph without drawing

In the original, the render walk also changes game state: every object's
transform and position in camera space (throwMatrix, cameraToObject), its
animation frame, frustum culling (which decides whether an object's children,
e.g. Mario's hands, are walked), the camera and every geo function it calls.
With SM64_DRAW false (platform/draw.h) the walk keeps all of that and draws
nothing: no display lists, no fixed point matrices, no viewport or display
list heap.

Two things drawing did that the game relies on:
- level of detail picks its children by the fixed point matrix's integer
  part of z: computed from the float matrix the same way, drawing or not
- building a shadow starts with find_floor, which clears
  gFindFloorIncludeSurfaceIntangible: done without the shadow

Files: `src/game/rendering_graph_node.c`

## 11. Geo functions without drawing

The geo functions that the render walk calls, with SM64_DRAW false:
- only drawing, skipped: skybox, mirror Mario's alpha and backface culling,
  cannon circle, castle lobby light, cake end screen, Bowser's coloring
- drawing and state, only the state: the flying carpet (gFlyingCarpetState),
  layer transparency (oAnimState), paintings (ripples, floors, the DDD
  painting's position), environment effects (particles move with the game's
  random numbers), moving textures (texture offsets and rotations)
- the rest only change state (camera, switch cases, Mario's body and hands,
  held objects, water levels) and run as before

The painting mesh comes from gEffectsMemoryPool and is freed in the same
call: that allocation and free leave the pool's free list as it was.

Files: `src/game/behaviors/bowser.inc.c`, `src/game/envfx_bubbles.c`, `src/game/envfx_snow.c`, `src/game/geo_misc.c`, `src/game/level_geo.c`, `src/game/mario_misc.c`, `src/game/moving_texture.c`, `src/game/object_helpers.c`, `src/game/paintings.c`, `src/game/screen_transition.c`

## 12. Skip the models that change no game state when not drawing

Most objects' models only draw: walking them computes matrices for every
part and changes nothing the game reads. Each graph node now caches (in
padding) whether its subtree holds anything that changes state (geo
functions other than the drawing-only ones, cameras, held and nested
objects) or a shadow. Without drawing, an object whose model has neither
gets only what the walk leaves in it: cameraToObject, which is the
translation row of its matrix computed with the same operations, and its
animation. A shadow that every walk of the model reaches only needs the
object's view test.

Files: `include/types.h`, `src/engine/graph_node.c`, `src/game/rendering_graph_node.c`

## 13. Hand the display list to the host when drawing

The host runs no RSP: a graphics task the game hands over goes nowhere.
When drawing, exec_display_list also keeps the task's display list for the
host (`gHostDrawnList`, platform/draw.h), which `sm64_step_draw` returns.

Files: `src/game/main.c`

## 14. Unpack the JP dialog font from the ROM's pixels, big-endian

The JP dialog font is 1 bit per pixel; render_generic_char unpacks each glyph
into the display list pool before drawing it, the only place the game's code
reads a texture's pixels. The library's textures are stand-ins
(tools/rom_stubs.py), so when drawing the glyph is read from the ROM
(`host_texture_pixels`), and its 16-bit words are read big-endian as the N64
does (a native build reads them byte-swapped). EU unpacks its 1-bit menu
font the same way (alloc_ia4_tex_from_i1), also from the ROM when drawing.
Nothing but the drawing reads the unpacked glyphs.

Files: `src/game/ingame_menu.c`

## 15. Mark the 3D camera in the display list when drawing

When drawing, geo_process_camera also puts a G_NOOP in the display list
carrying the camera's matrix (SM64_CAMERA_TAG, include/sm64_physics.h), so a
renderer can draw the scene from a camera of its own. The matrix is allocated
from the display list pool like the game's own.

Files: `src/game/rendering_graph_node.c`

## 16. Run the Shindou Edition's rumble thread at every vertical interrupt

The Shindou Edition's rumble thread (thread6) waits for vertical interrupts,
counts down gRumblePakTimer (which Mario's code reads) and, without a Rumble
Pak, looks for one every 60 of them. The host runs no threads: its start and
one iteration of its loop are functions of their own (rumble_thread_start,
rumble_thread_vi), which the host runs where the N64 would, and the host
counts gNumVblanks, two per frame. Its audio loader and rumble code include
the SDK's whole PR/os.h, which declares libultra with 32-bit addresses; they
include what they use instead.

Files: `src/game/rumble_init.c`, `src/game/rumble_init.h`, `src/audio/load_sh.c`

## 17. Run the sound thread while EU and Shindou wait for its reset

EU and the Shindou Edition reset the sound session by sending the sound thread
a request and blocking until it answers (audio_reset_session_eu). Without
threads the answer would never come: the host's blocking receive returns at
once, and the game went on while the sound thread had not reset yet, which
cut off the music it started next. With sound on, the host runs the sound
thread's work until the answer is there, as the N64 would during the wait
(docs/changes.md 2 does the same for wait_for_audio_frames). Without sound
nothing changes.

Files: `src/audio/external.c`

## 18. Build the sound thread's command lists as host commands

The sound thread writes its RSP command list through `u64 *` pointers, one
64-bit word per command, as on the N64. The host's commands (Acmd, abi.h) are
two pointer-sized words: each overwrote half of the one before. The list
pointers are `Acmd *` and the buffers are allocated with sizeof(Acmd); the
task's data size still counts the commands in u64 units, as the RSP reads it.
Nothing but the RSP reads the list.

Files: `src/audio/synthesis.c`, `src/audio/synthesis.h`, `src/audio/synthesis_sh.c`,
`src/audio/data.c`, `src/audio/data.h`, `src/audio/heap.c`, `src/audio/external.c`,
`src/audio/port_eu.c`, `src/audio/port_sh.c`

## 19. Receive the Shindou sound thread's messages as messages

The Shindou Edition's sound code receives messages into 32-bit integers
through a cast (`osRecvMesg(q, (OSMesg *) &x, ...)`). An OSMesg is as wide
as an address, 8 bytes on a 64-bit host, and the receive wrote past the
integer. It receives into an OSMesg and takes the integer from it. Only the
sound thread receives these.

Files: `src/audio/port_sh.c`, `src/audio/load_sh.c`

## 20. Read the sound data from the user's ROM

The sound banks, sample banks, sequences and bank sets are not part of the
library: the decomp builds them from the ROM, and the library converts them
from the user's ROM when it is loaded (platform/sound.c), into the host's
layout. The audio loader's names for them are pointers to the converted
copies instead of arrays. The Shindou Edition's headers and bank sets are the
loader's own arrays, filled from the ROM the same way.

Files: `src/audio/load.c`, `src/audio/load_sh.c`

## 21. Draw Mario alone

With `sm64_set_draw_mario_only`, a drawing step keeps only the display lists
appended while the render walk is inside Mario's object (his model, what he
holds, his shadow), leaves out the frame's color clear, and skips the 2D drawn
over the scene (HUD, text, dialogs, menus, screen transitions). A renderer
draws it over another world's frame to show this world's Mario as a ghost. It
is meant for a copy of the world: the skipped menu and transition code also
updates state.

Files: `src/game/rendering_graph_node.c`, `src/game/game_init.c`, `src/game/area.c`

## 22. Draw the sky wide enough for a wide view

The sky is a grid of three by three tiles of the panorama around the camera,
enough for a 4:3 view. With `sm64_set_draw_widescreen`, a drawing step adds two
columns on each side (the panorama wraps around every eight) and allocates
their commands, so a renderer that widens the view (games/sm64's f3d/) has sky
to its edges. Nothing changes otherwise.

Files: `src/game/skybox.c`

## 23. Update a rippling painting's ripple without drawing

Drawing a rippling painting ends with painting_update_ripple_state, which
decays the ripple and returns the painting to idle once it is small; only an
idle painting ripples again or records the height Mario jumps in at, which
sets Wet-Dry World's water level. A step without drawing skipped it with the
drawing, so a painting that had rippled once never stopped: the 70-star TAS
entered Wet-Dry World with the water of an earlier jump. Without drawing, a
rippling painting's ripple is updated where drawing would.

Files: `src/game/paintings.c`

## 24. Draw between two frames

With `sm64_set_draw_interpolation`, a drawing step draws its frame between
the world it started from and the one it leaves, for a renderer that shows
more than the game's 30 frames a second. Objects (their position, angle and
scale, and a throw matrix moved with them), the camera and the sky's camera
are where they were between the two frames. The values come from the other
world at the same place in its memory: a pool object is interpolated only if
it was the same object there (active, same behavior), and nothing that moved
further than 1000 units, nor anything when the level or area changed. The
values the game keeps are not touched; a step that does not draw is the same.

Files: `src/game/rendering_graph_node.c`, `src/game/level_geo.c`

## 25. Read Wiggler's first walking speed where the N64 does

On JP, US and the Shindou Edition, Wiggler's first frame of walking reads his
target speed as `sWigglerSpeeds[health - 1]` with the health of 2048 he
spawned with (5): the float 2047 entries past the array. On the N64 that is
the audio data after it, the same on every console: `gVolRampingLhs144[80]`
(113762.27) on JP and US, `gStereoPanVolume[91]` on the Shindou Edition. So
he starts walking at 1 unit a frame. Natively the read landed in whatever the
library's layout put there, a tiny number, and he stood still for a frame:
the 70-star TAS differed from Wiggler's fight on. The speed is now read from
those tables.

Files: `src/game/behaviors/wiggler.inc.c`, `src/game/obj_behaviors_2.c`
