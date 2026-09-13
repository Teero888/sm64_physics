#include <stdio.h>
#include <stdlib.h>
#include "sm64.h"
#include "game/area.h"
#include "game/mario.h"
#include "game/mario_step.h"
#include "../host/terrain.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

int main(void) {
    struct sm64_terrain_triangle triangle = {
        .vertices = {{-1000,0,-1000}, {0,0,1000}, {1000,0,-1000}}, .type = SURFACE_DEFAULT};
    struct sm64_terrain *terrain = sm64_terrain_create(&triangle, 1, NULL, 0);
    CHECK(terrain != NULL);
    sm64_terrain_activate(terrain);
    struct Object object = {0};
    struct Area area = {.terrainType = TERRAIN_GRASS};
    struct MarioBodyState body = {0};
    struct MarioState mario = {.marioObj = &object, .area = &area, .marioBodyState = &body,
        .action = ACT_IDLE, .actionState = 7, .actionTimer = 20, .forwardVel = 16, .waterLevel = -11000};
    mario.floorHeight = find_floor(0, 0, 0, &mario.floor);
    CHECK(set_mario_action(&mario, ACT_JUMP, 3));
    CHECK(mario.prevAction == ACT_IDLE && mario.action == ACT_JUMP && mario.actionArg == 3);
    CHECK(mario.actionState == 0 && mario.actionTimer == 0 && mario.vel[1] == 46);
    CHECK(fabsf(mario.forwardVel - 12.8f) < 0.00001f);
    CHECK(object.header.gfx.animInfo.animID == -1);
    mario.input = INPUT_A_DOWN;
    int result = AIR_STEP_NONE, frames = 0;
    while (result != AIR_STEP_LANDED && frames++ < 60) result = perform_air_step(&mario, 0);
    CHECK(result == AIR_STEP_LANDED && mario.pos[1] == 0 && frames > 10);
    CHECK(set_mario_action(&mario, ACT_IDLE, 0));
    CHECK(mario.prevAction == ACT_JUMP);

    mario.forwardVel = 100;
    set_mario_action(&mario, ACT_LONG_JUMP, 0);
    CHECK(mario.forwardVel == 48 && mario.vel[1] == 30);
    mario.forwardVel = -100;
    set_mario_action(&mario, ACT_LONG_JUMP, 0);
    CHECK(mario.forwardVel == -150); /* Original backwards long-jump behavior. */
    mario.forwardVel = 0;
    mario.squishTimer = 1;
    set_mario_action(&mario, ACT_DOUBLE_JUMP, 0);
    CHECK(mario.action == ACT_JUMP && mario.vel[1] == 21);
    mario.squishTimer = 0;
    mario.intendedMag = 32;
    mario.forwardVel = 0;
    set_mario_action(&mario, ACT_WALKING, 0);
    CHECK(mario.forwardVel == 8);
    mario.floor->type = SURFACE_VERY_SLIPPERY;
    mario.forwardVel = 0;
    set_mario_action(&mario, ACT_WALKING, 0);
    CHECK(mario.forwardVel == 0);
    set_mario_action(&mario, ACT_METAL_WATER_JUMP, 0);
    CHECK(mario.vel[1] == 32);
    mario.vel[1] = 7;
    set_mario_action(&mario, ACT_WATER_JUMP, 1);
    CHECK(mario.vel[1] == 7);
    set_mario_action(&mario, ACT_EMERGE_FROM_PIPE, 0);
    CHECK(mario.vel[1] == 52);
    sm64_terrain_destroy(terrain);
    puts("Original action setup: jump flight, reset state, BLJ speeds, squish and surface effects passed");
    return 0;
}
