// What a caller reads of a world without the game's headers: Mario, the
// camera, and any variable's copy in a world (include/sm64_physics.h). Not
// part of the state; it reads a world's through WORLD() while it is current.
#include <ultra64.h>
#include <string.h>

#include "sm64_physics.h"
#include "types.h"
#include "game/area.h"
#include "game/camera.h"
#include "game/game_init.h"
#include "game/level_update.h"
#include "game/object_list_processor.h"

extern char sm64_state_start[], sm64_state_end[]; // platform/state.ld
extern struct CameraFOVStatus sFOVState;           // game/camera.c

const char *sm64_version(void) {
#ifdef VERSION_JP
    return "jp";
#else
    return "us";
#endif
}

void *sm64_world_variable(const sm64_world *world, const void *variable) {
    const char *p = variable;
    if (p < sm64_state_start || p >= sm64_state_end) {
        return (void *) variable;
    }
    const ptrdiff_t offset = gHostWorldOffset;
    sm64_world_enter(world);
    void *copy = (char *) p + gHostWorldOffset;
    gHostWorldOffset = offset;
    return copy;
}

bool sm64_mario(const sm64_world *world, struct sm64_mario_info *out) {
    const struct MarioState *m = sm64_world_variable(world, &gMarioStates[0]);
    memset(out, 0, sizeof(*out));
    for (int i = 0; i < 3; ++i) {
        out->pos[i] = m->pos[i];
        out->vel[i] = m->vel[i];
        out->face_angle[i] = m->faceAngle[i];
    }
    out->forward_vel = m->forwardVel;
    out->action = m->action;
    out->action_state = m->actionState;
    out->action_timer = m->actionTimer;
    out->health = m->health;
    out->num_stars = m->numStars;
    out->num_coins = m->numCoins;
    out->num_lives = m->numLives;
    out->level = *(const s16 *) sm64_world_variable(world, &gCurrLevelNum);
    out->area = *(const s16 *) sm64_world_variable(world, &gCurrAreaIndex);
    out->global_timer = *(const u32 *) sm64_world_variable(world, &gGlobalTimer);
    // Mario exists once a level has spawned him.
    return *(struct Object *const *) sm64_world_variable(world, &gMarioObject) != NULL;
}

void sm64_camera(const sm64_world *world, struct sm64_camera_info *out) {
    const struct LakituState *l = sm64_world_variable(world, &gLakituState);
    const struct CameraFOVStatus *fov = sm64_world_variable(world, &sFOVState);
    for (int i = 0; i < 3; ++i) {
        out->pos[i] = l->pos[i];
        out->focus[i] = l->focus[i];
    }
    out->roll = l->roll;
    out->fov = fov->fov;
}
