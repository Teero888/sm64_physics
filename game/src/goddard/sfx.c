#include <PR/ultratypes.h>

#include "sfx.h"

static u32 sCurrSfx;  // bitset of currently playing sound effects
static u32 sPrevSfx;  // bitset of sound effects that were playing on the previous frame

/**
 * Stops all sound effects
 */
void gd_reset_sfx(void) {
    WORLD(sPrevSfx) = GD_SFX_NONE;
    WORLD(sCurrSfx) = GD_SFX_NONE;
}

/**
 * Returns a bitset of the newly started sound effects.
 * This is used by geo_draw_mario_head_goddard to start new sounds.
 */
u32 gd_new_sfx_to_play(void) {
    return ~WORLD(sPrevSfx) & WORLD(sCurrSfx);
}

/**
 * Called at the start of a frame.
 */
void gd_sfx_update(void) {
    WORLD(sPrevSfx) = WORLD(sCurrSfx);
    WORLD(sCurrSfx) = GD_SFX_NONE;
}

/**
 * Marks a sound effect to be played. This must be called repeatedly once per
 * frame to keep the sound effect playing.
 */
void gd_play_sfx(enum GdSfx sfx) {
    WORLD(sCurrSfx) |= sfx;
}

// Library: its variables' addresses (tools/state/types.py).
#include "pointers/game/src/goddard/sfx.c.inc.c"
