// Whether the game's render code draws. On the N64, each frame's render walk
// also updates game state (the camera, animation frames, geo functions); the
// patches make the drawing in it conditional on SM64_DRAW, so a game step that
// does not draw still updates the same state.
#ifndef SM64_PLATFORM_DRAW_H
#define SM64_PLATFORM_DRAW_H

extern __thread int gHostDraw __attribute__((tls_model("initial-exec")));
#define SM64_DRAW gHostDraw

// While drawing: draw Mario alone (sm64_set_draw_mario_only), and whether the
// render walk is inside his object now.
extern __thread int gHostDrawMarioOnly __attribute__((tls_model("initial-exec")));
extern __thread int gHostDrawInsideMario __attribute__((tls_model("initial-exec")));
#define SM64_DRAW_MARIO_ONLY (gHostDraw && gHostDrawMarioOnly)

// While drawing: the view is wider than 4:3 (sm64_set_draw_widescreen).
extern __thread int gHostDrawWideSetting __attribute__((tls_model("initial-exec")));
#define gHostDrawWide (gHostDraw && gHostDrawWideSetting)

// The display list of the last graphics task the game handed over while
// drawing (exec_display_list): what the RSP runs.
extern __thread const void *gHostDrawnList __attribute__((tls_model("initial-exec")));

// A texture's pixels: from the ROM for the library's stand-ins
// (platform/rom.c), the address itself for any other.
const void *host_texture_pixels(const void *address);

#endif
