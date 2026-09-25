// Whether the game's render code draws. On the N64, each frame's render walk
// also updates game state (the camera, animation frames, geo functions); the
// patches make the drawing in it conditional on SM64_DRAW, so a game step that
// does not draw still updates the same state.
#ifndef SM64_PLATFORM_DRAW_H
#define SM64_PLATFORM_DRAW_H

extern __thread int gHostDraw __attribute__((tls_model("initial-exec")));
#define SM64_DRAW gHostDraw

#endif
