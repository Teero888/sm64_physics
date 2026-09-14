#include <assert.h>
#include "../host/camera.h"
#include "../host/terrain.h"
#include "../host/audio.h"
#include "game/camera.h"
#include "game/area.h"
#include "game/mario.h"
#include "game/mario_step.h"

struct observation { int calls; s16 mode, frames, previous; };
/* Test observer applies mode bookkeeping only, not a production camera. */
static void observe(void *user, struct Camera *camera, s16 mode, s16 frames) {
    struct observation *o = user;
    ++o->calls;
    o->mode = mode;
    o->frames = frames;
    s16 next = mode == -1 ? o->previous : mode;
    o->previous = camera->mode;
    camera->mode = next;
}
int main(void) {
    struct sm64_objects *objects = sm64_objects_create();
    struct sm64_terrain *terrain = sm64_terrain_create(NULL, 0, NULL, 0);
    struct sm64_audio_state *audio = sm64_audio_create(NULL, NULL);
    assert(objects && terrain && audio);
    sm64_objects_activate(objects);
    sm64_terrain_activate(terrain);
    sm64_audio_activate(audio);
    struct observation seen = {.previous = CAMERA_MODE_CLOSE};
    sm64_objects_bind_camera(objects, observe, &seen);
    struct Camera camera = {.mode = CAMERA_MODE_CLOSE, .defMode = CAMERA_MODE_FREE_ROAM};
    struct Area area = {.camera = &camera, .terrainType = TERRAIN_GRASS};
    struct Object player = {0};
    struct Surface floor = {.type = SURFACE_DEFAULT, .normal = {0,1,0}};
    struct MarioState m = {.marioObj = &player, .area = &area, .floor = &floor,
        .action = ACT_FREEFALL, .forwardVel = 32, .vel = {0,-40,0}, .waterLevel = 500,
        .faceAngle = {100,200,300}, .angleVel = {1,2,3}};
    assert(set_water_plunge_action(&m));
    assert(m.action == ACT_WATER_PLUNGE && m.forwardVel == 8 && m.vel[1] == -20);
    assert(m.pos[1] == 400 && !m.faceAngle[0] && !m.faceAngle[2]);
    assert(!m.angleVel[0] && !m.angleVel[1] && !m.angleVel[2]);
    assert(seen.calls == 1 && seen.mode == CAMERA_MODE_WATER_SURFACE && seen.frames == 1);
    assert(transition_submerged_to_walking(&m) && m.action == ACT_WALKING);
    assert(seen.calls == 2 && camera.mode == camera.defMode);
    m.action = ACT_FIRST_PERSON;
    seen.previous = CAMERA_MODE_CLOSE;
    sm64_terrain_set_camera_movement_flags(terrain, CAM_MOVE_C_UP_MODE);
    update_mario_sound_and_camera(&m);
    assert(seen.mode == -1 && camera.mode == CAMERA_MODE_CLOSE);
    assert(!(sm64_terrain_camera_movement_flags(terrain) & CAM_MOVE_C_UP_MODE));
    m.action = ACT_IDLE;
    floor.type = SURFACE_SHALLOW_QUICKSAND;
    m.quicksandDepth = 9;
    assert(!mario_update_quicksand(&m, 2) && m.quicksandDepth == 10);
    floor.type = SURFACE_DEEP_QUICKSAND;
    m.quicksandDepth = 159;
    camera.mode = CAMERA_MODE_WATER_SURFACE;
    assert(mario_update_quicksand(&m, 2) && m.action == ACT_QUICKSAND_DEATH);
    assert(camera.mode == camera.defMode);
    m.action = ACT_RIDING_SHELL_GROUND;
    floor.type = SURFACE_INSTANT_QUICKSAND;
    assert(!mario_update_quicksand(&m, 2) && m.quicksandDepth == 0);
    sm64_audio_activate(NULL);
    sm64_terrain_activate(NULL);
    sm64_objects_activate(NULL);
    sm64_audio_destroy(audio);
    sm64_terrain_destroy(terrain);
    sm64_objects_destroy(objects);
    return 0;
}
