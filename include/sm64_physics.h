// Native Super Mario 64: the n64decomp/sm64 game code stepped one game frame
// at a time. One instance per process for now.
#ifndef SM64_PHYSICS_H
#define SM64_PHYSICS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Power-on: what the console does before the first game frame. sm64_step
// calls it on first use.
void sm64_boot(void);

// One game frame. input is the controller as read by that frame, in
// mupen64plus's BUTTONS layout (an .m64 sample): buttons in bits 0-15, stick X
// in bits 16-23 and stick Y in bits 24-31.
void sm64_step(uint32_t input);

// Whether the sound thread runs: off by default. The game state does not
// depend on it; it only mixes the music and sound effects.
void sm64_set_audio(bool enabled);

// Whether sm64_step also builds the frame's display lists, as the N64 does:
// off by default. The game state does not depend on it; turning it on checks
// exactly that.
void sm64_set_draw(bool enabled);

#ifdef __cplusplus
}
#endif

#endif
