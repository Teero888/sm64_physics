# AVOID_UB and other native differences

The decomp requires `AVOID_UB` for non-N64 builds. At every site it replaces
behavior C leaves undefined with something defined, which is not always what
the N64 did. This is the audit of the game code (src/game, src/engine,
src/menu), JP, against the disassembly of the matching build. Audio and Goddard
sites only affect sound and the title screen's Mario head and are not audited.

| Site | What AVOID_UB does | N64 | Status |
|---|---|---|---|
| `object_collision.c` `detect_object_hitbox_overlap`, `detect_object_hurtbox_overlap` | missing return is 0 | returns v0: the previous result of either in the collision pass | **patch 0004** |
| `wiggler.inc.c` init | sets health to 4 | JP/US read 0 (fields are zeroed at spawn); only EU sets 4 | **patch 0005** |
| `camera.c` `nop_update_water_camera` | missing return is 0 | returns v0: the low half of `set_camera_mode`'s stack pointer, left by `vec3f_copy` returning `&dest`. Stored into `sAreaYaw` when entering the water surface mode (`0x6e78`, `0x6eb0` in the JP 1-key TAS, depending on the call path) | **patch 0007**, from the N64 stack pointer model (`platform/n64stack.h`) |
| `camera_lakitu.inc.c` intro dialog | target pitch/yaw start at 0 | uninitialized registers, read while Lakitu hovers during his dialog | matches the emulator in the JP 1-key TAS (new file, full intro) |
| `mario_actions_airborne.c` wall kick | returns `set_mario_animation`'s result | same value is in v0 | faithful |
| `shadow.c` water shadow height | returns `waterLevel` | same | faithful (rendering) |
| `math_util.h` `gCosineTable` | part of `gSineTable` | the two tables are adjacent | same values |
| `geo_switch_anim_state`, `geo_switch_area`, `geo_act_selector_strings` | third parameter declared | o32 ignores it | no effect |
| `bowling_ball`, `mips`, `snowman`, `manta_ray` `followStatus` | initialized to 0 | uninitialized, but the callee ignores it | no effect |
| `chuckya.inc.c` `sp3C` | initialized | only printed by a debug function | no effect |
| `camera.c` `radial_camera_input` return | `dummy` initialized | uninitialized return value, ignored by every caller | no effect |
| `camera.c` `unused_update_mode_5_camera` | missing return is 0 | unused camera mode | no effect |
| `file_select.c` `lvl_init_menu_values_and_cursor_pos`, `star_select.c` `lvl_init_act_selector_values_and_stars` | missing return is 0 | the level script's register is overwritten by the next `CALL_LOOP` before it is read | no effect |
| `paintings.c` ripple origin helpers | missing return is 0 | unreachable default cases | no effect |
| `screen_transition.c`, `intro_geo.c`, `main.c`, `framebuffers.c`, `mtxf_to_mtx` | defined values | rendering or boot only | no effect |
| `file_select.c` `NUM_BUTTONS` | EU only | | not JP |

## Other differences of a native build

| | N64 | Native | Status |
|---|---|---|---|
| Goddard/menu segment reloads (`FIXED_LOAD`) | all of src/menu's and src/goddard's variables return to their initial values | kept their values | **patch 0006** and `host_reload_overlay` |
| `GraphNodeCamera.config` union | pointer and mode are both 32 bits | pointer's upper half uninitialized | **patch 0003** |
| `load_to_fixed_pool_addr` | allocates the segment at the right end of the main pool | nothing | pool addresses differ anyway; only matters if the pool runs out |
| Object fields used as two s16 | `asS16[i][0]` is the upper half of the slot | the lower half | each side reads its own layout; only the comparator cares |

## The N64 stack pointer model

Where the original leaks a stack address into game state, the value depends
on how deep the call path is. `platform/n64stack.c` keeps the N64 game
thread's stack pointer: the files on the call path from the game loop to such
a place are compiled with `-finstrument-functions` through wrappers generated
by `tools/n64stack/frames.py`, which register every function's N64 frame size
read from the procedure descriptors in the matching build's `.mdebug` data.
The host sets the pointer to its value inside `thread5_game_loop` (the game
thread's stack top, less 16 as `osCreateThread` does, less that function's
frame) before running the level script. Both values seen in the JP 1-key TAS
come out exactly, without calibration.

Instrumented: `level_script.c`, `level_update.c`, `area.c`,
`object_list_processor.c`, `behavior_script.c`, `mario.c`, `interaction.c`,
`mario_actions_*.c`. A function missing from that path would show up as a
wrong value in the lockstep comparison.
