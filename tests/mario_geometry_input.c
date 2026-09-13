#include <stdio.h>
#include <stdlib.h>
#include "sm64.h"
#include "game/area.h"
#include "game/level_update.h"
#include "../host/terrain.h"
#include "../host/mario_motion.h"
#include "../host/controller.h"
#include "game/camera.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

/* Test observation of the transition dependency. This is not a production
 * warp implementation and does not claim to execute a level transition. */
static int warp_count;
static s32 warp_operation;
static int debug_updates;
void debug_print_speed_action_normal(struct MarioState *m) { (void)m; ++debug_updates; }
s16 level_trigger_warp(struct MarioState *m, s32 operation) {
    CHECK(m->floor == NULL);
    ++warp_count;
    warp_operation = operation;
    return 0;
}

int main(void) {
    struct sm64_terrain_triangle surfaces[] = {
        {.vertices = {{-1000,0,-1000}, {0,0,1000}, {1000,0,-1000}}, .type = SURFACE_DEFAULT},
        {.vertices = {{-1000,120,-1000}, {1000,120,-1000}, {0,120,1000}},
         .type = SURFACE_DEFAULT, .dynamic = true}
    };
    struct sm64_terrain_region regions[] = {
        {0, -500, -500, 500, 500, 100}, {50, -500, -500, 500, 500, 300}
    };
    struct Object object = {0};
    struct Area area = {.terrainType = TERRAIN_GRASS};
    struct MarioState mario = {.marioObj = &object, .area = &area, .action = ACT_IDLE};
    struct sm64_terrain *terrain = sm64_terrain_create(surfaces, 1, regions, 2);
    CHECK(terrain != NULL);
    sm64_terrain_activate(terrain);
    mario.pos[1] = 10;
    update_mario_geometry_inputs(&mario);
    CHECK(mario.floorHeight == 0 && mario.waterLevel == 100);
    CHECK((mario.input & (INPUT_IN_WATER | INPUT_IN_POISON_GAS)) == (INPUT_IN_WATER | INPUT_IN_POISON_GAS));
    mario.input = 0;
    mario.pos[1] = 200;
    update_mario_geometry_inputs(&mario);
    CHECK((mario.input & INPUT_OFF_FLOOR) && !(mario.input & (INPUT_IN_WATER | INPUT_IN_POISON_GAS)));
    mario.pos[0] = 9000;
    object.header.gfx.pos[1] = 10;
    update_mario_geometry_inputs(&mario);
    CHECK(mario.pos[0] == 0 && mario.pos[1] == 10 && warp_count == 0);
    mario.pos[0] = object.header.gfx.pos[0] = 9000;
    update_mario_geometry_inputs(&mario);
    CHECK(mario.floor == NULL && warp_count == 1 && warp_operation == WARP_OP_DEATH);
    sm64_terrain_destroy(terrain);

    terrain = sm64_terrain_create(surfaces, 2, NULL, 0);
    CHECK(terrain != NULL);
    sm64_terrain_activate(terrain);
    mario.pos[0] = mario.pos[1] = 0;
    mario.input = 0;
    update_mario_geometry_inputs(&mario);
    CHECK(mario.ceilHeight == 120 && (mario.input & INPUT_SQUISHED));
    sm64_terrain_destroy(terrain);

    surfaces[0].vertices[0][1] = -1000;
    surfaces[0].vertices[2][1] = 1000;
    terrain = sm64_terrain_create(surfaces, 1, NULL, 0);
    CHECK(terrain != NULL);
    sm64_terrain_activate(terrain);
    mario.input = 0;
    update_mario_geometry_inputs(&mario);
    CHECK(mario.input & INPUT_ABOVE_SLIDE);
    CHECK(warp_count == 1);
    struct Controller controller = {0};
    struct Camera camera = {0};
    area.camera = &camera;
    mario.controller = &controller;
    mario.wallKickTimer = 3;
    mario.doubleJumpTimer = 4;
    mario.particleFlags = 0xffffffff;
    mario.input = 0xffff;
    sm64_controller_update(&controller, A_BUTTON, 70, 0);
    update_mario_inputs(&mario);
    CHECK(mario.particleFlags == 0 && (mario.input & INPUT_A_PRESSED));
    CHECK(mario.input & INPUT_NONZERO_ANALOG);
    CHECK(mario.wallKickTimer == 2 && mario.doubleJumpTimer == 3 && debug_updates == 1);
    sm64_controller_update(&controller, 0, 0, 0);
    update_mario_inputs(&mario);
    CHECK(!(mario.input & (INPUT_A_PRESSED | INPUT_A_DOWN | INPUT_NONZERO_ANALOG)));
    CHECK(mario.input & INPUT_UNKNOWN_5);
    CHECK(debug_updates == 2 && warp_count == 1);
    sm64_terrain_set_camera_movement_flags(terrain, CAM_MOVE_C_UP_MODE);
    update_mario_inputs(&mario);
    CHECK(mario.input & INPUT_FIRST_PERSON);
    mario.action = ACT_JUMP;
    update_mario_inputs(&mario);
    CHECK(!(mario.input & INPUT_FIRST_PERSON));
    CHECK(!(sm64_terrain_camera_movement_flags(terrain) & CAM_MOVE_C_UP_MODE));
    sm64_terrain_destroy(terrain);
    puts("Original geometry inputs: water, gas, slopes, crushing, OOB recovery and death requests passed");
    return 0;
}
