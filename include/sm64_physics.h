// Native Super Mario 64: the whole game, stepped one frame at a time,
// bit-identical to the console. Any number of worlds, each a console of its
// own; threads may step different worlds at the same time.
#ifndef SM64_PHYSICS_H
#define SM64_PHYSICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The user's ROM (.z64, .v64 or .n64 byte order) of the version this library
// is built for: the library carries the game, but not what the decomp takes
// from the ROM. Returns false if it is not that ROM. Worlds created before it
// have no demo inputs on the title screen.
bool sm64_load_rom(const void *rom, size_t size);

// A console just powered on, before its first game frame.
typedef struct sm64_world sm64_world;
sm64_world *sm64_world_create(void);
void sm64_world_destroy(sm64_world *world);

// One game frame. input is the controller as read by that frame, in
// mupen64plus's BUTTONS layout (an .m64 sample): buttons in bits 0-15, stick X
// in bits 16-23 and stick Y in bits 24-31. A world is stepped by one thread
// at a time.
void sm64_step(sm64_world *world, uint32_t input);

// Makes world the calling thread's current one: the game's variables (as the
// game's headers declare them, through WORLD()) are then that world's.
void sm64_world_enter(const sm64_world *world);

// Settings of the calling thread, not state. Whether a step runs the sound
// thread (off by default; the game does not depend on it, it only mixes
// sound), and whether it also builds the frame's display lists as the N64
// does (off by default; the game state does not depend on it either).
void sm64_set_audio(bool enabled);
void sm64_set_draw(bool enabled);

// A world's whole state: everything a step reads and writes. A saved state
// holds pointers into its world's memory: it can only be loaded into the
// world that saved it.
size_t sm64_state_size(void);
void sm64_save_state(const sm64_world *world, void *buffer);
void sm64_load_state(sm64_world *world, const void *buffer);

#ifdef __cplusplus
}
#endif

#endif
