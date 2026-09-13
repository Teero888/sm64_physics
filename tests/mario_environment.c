#include <assert.h>
#include <math.h>
#include "../host/terrain.h"
#include "../host/mario_status.h"
#include "../host/audio.h"
#include "game/area.h"
#include "game/mario_step.h"

static void near(float actual, float expected) { assert(fabsf(actual - expected) < 0.001f); }
int main(void) {
    struct sm64_terrain_triangle triangle = {
        .vertices = {{-1000,0,-1000}, {0,0,1000}, {1000,0,-1000}}, .type = SURFACE_DEFAULT};
    struct sm64_terrain *terrain = sm64_terrain_create(&triangle, 1, NULL, 0);
    struct sm64_objects *objects = sm64_objects_create();
    struct sm64_audio_state *audio = sm64_audio_create(NULL, NULL);
    assert(terrain && objects && audio);
    sm64_terrain_activate(terrain);
    sm64_objects_activate(objects);
    sm64_audio_activate(audio);
    struct Object player = {0};
    struct Area area = {.terrainType = TERRAIN_GRASS};
    struct MarioState m = {.marioObj = &player, .area = &area, .action = ACT_IDLE,
        .waterLevel = -11000};
    m.floorHeight = find_floor(0, 0, 0, &m.floor);
    m.pos[1] = 100;
    m.forwardVel = 20;
    assert(stationary_ground_step(&m) == GROUND_STEP_NONE);
    assert(m.pos[1] == 0 && m.forwardVel == 0 && player.header.gfx.pos[1] == 0);
    m.floor->type = SURFACE_MOVING_QUICKSAND;
    m.floor->force = 0;
    assert(stationary_ground_step(&m) == GROUND_STEP_NONE);
    near(m.pos[2], 12);
    m.floor->force = 0x140; /* Medium sand current toward +X. */
    assert(stationary_ground_step(&m) == GROUND_STEP_NONE);
    near(m.pos[0], 8);
    near(m.pos[2], 12);
    m.floor->type = SURFACE_HORIZONTAL_WIND;
    m.floor->force = 0;
    sm64_objects_set_frame_info(objects, 3, 0);
    assert(stationary_ground_step(&m) == GROUND_STEP_NONE);
    near(m.pos[2], 18.2f);
    m.action = ACT_WALKING;
    m.forwardVel = 20;
    m.vel[0] = m.vel[2] = 0;
    assert(mario_update_windy_ground(&m));
    near(m.vel[2], 10);
    m.vel[1] = 30;
    m.pos[1] = 100;
    stop_and_set_height_to_floor(&m);
    assert(m.pos[1] == 0 && m.vel[1] == 0 && m.forwardVel == 0);
    m.forwardVel = 10;
    mario_bonk_reflection(&m, TRUE);
    assert(m.forwardVel == -10);
    struct BullyCollisionData a = {.posX = 0, .velX = 5, .conversionRatio = 1};
    struct BullyCollisionData b = {.posX = 10, .velX = -2, .conversionRatio = 1};
    transfer_bully_speed(&a, &b);
    near(a.velX, -2);
    near(b.velX, 5);
    sm64_audio_activate(NULL);
    sm64_objects_activate(NULL);
    sm64_terrain_activate(NULL);
    sm64_audio_destroy(audio);
    sm64_objects_destroy(objects);
    sm64_terrain_destroy(terrain);
    return 0;
}
