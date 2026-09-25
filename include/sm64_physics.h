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

// The library exports the game's own functions and variables with protected
// visibility; this header's, with default visibility, are its API.
#pragma GCC visibility push(default)

// The game version this library is built for: "jp" or "us".
const char *sm64_version(void);

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

// Makes dst the same console as src (both of this process). A world is
// copied while no thread steps it; it keeps its own memory.
void sm64_world_copy(sm64_world *dst, const sm64_world *src);
sm64_world *sm64_world_clone(const sm64_world *src);

// A world's whole state as bytes: everything a step reads and writes. It
// loads into any world of the process that saved it (it holds addresses of
// the library's code and data); false if buffer is not a saved state.
size_t sm64_state_size(void);
void sm64_save_state(const sm64_world *world, void *buffer);
bool sm64_load_state(sm64_world *world, const void *buffer);

// --- Drawing ---------------------------------------------------------------------

// One game frame, as sm64_step, with the game drawing: returns the display
// list the frame hands to the RSP (Fast3D, in the host's layout of the decomp's
// gbi.h: 64-bit words, addresses in the world's memory and the library's), or
// NULL if the frame started none. Valid until the world changes. The world's
// state after it is the same as after sm64_step. To draw a frame already
// stepped, step a copy of the world before it (sm64_world_copy).
const void *sm64_step_draw(sm64_world *world, uint32_t input);

// The pixels of a texture a display list names, from the ROM (sm64_load_rom):
// the library's textures are stand-ins of the right size that say where their
// pixels are. NULL for an address that is not one of them (a texture the game
// makes itself), or before the ROM is loaded. Valid until sm64_load_rom.
const void *sm64_texture(const void *address);

// --- Reading a world ------------------------------------------------------------

// Mario as the game keeps him (gMarioStates[0] and a few globals). Returns
// false before a level has spawned him.
struct sm64_mario_info {
    float pos[3], vel[3], forward_vel;
    int16_t face_angle[3];
    uint32_t action;
    uint16_t action_state, action_timer;
    int16_t health, num_stars, num_coins, num_lives;
    int16_t level, area; // gCurrLevelNum, gCurrAreaIndex
    uint32_t global_timer;
};
bool sm64_mario(const sm64_world *world, struct sm64_mario_info *out);

// Lakitu, the game's camera: where it is, what it looks at, its roll (an
// angle, 0x10000 a turn) and its vertical field of view in degrees.
struct sm64_camera_info {
    float pos[3], focus[3];
    int16_t roll;
    float fov;
};
void sm64_camera(const sm64_world *world, struct sm64_camera_info *out);

// World's copy of one of the game's variables, given the variable's own
// address as the game's headers declare it (&gMarioStates[0], &gCurrLevelNum):
// for code that reads the game directly. Addresses outside the state (code,
// constant data) are returned as they are.
void *sm64_world_variable(const sm64_world *world, const void *variable);

#pragma GCC visibility pop

#ifdef __cplusplus
}
#endif

#endif
