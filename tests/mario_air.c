#include <stdio.h>
#include <stdlib.h>
#include "sm64.h"
#include "game/area.h"
#include "game/mario_step.h"
#include "../host/terrain.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

int main(void) {
    struct sm64_terrain_triangle surfaces[] = {
        {.vertices = {{-1000,0,-1000}, {0,0,1000}, {1000,0,-1000}}, .type = SURFACE_DEFAULT},
        {.vertices = {{-1000,200,-1000}, {1000,200,-1000}, {0,200,1000}}, .type = SURFACE_HANGABLE},
        {.vertices = {{100,0,-1000}, {100,0,1000}, {100,1000,0}}, .type = SURFACE_BURNING}
    };
    struct Object object = {0};
    struct Area area = {.terrainType = TERRAIN_GRASS};
    struct MarioBodyState body = {0};
    struct MarioState mario = {.marioObj = &object, .area = &area,
        .marioBodyState = &body, .action = ACT_FREEFALL, .waterLevel = -11000};
    struct sm64_terrain *terrain = sm64_terrain_create(surfaces, 1, NULL, 0);
    CHECK(terrain != NULL);
    sm64_terrain_activate(terrain);
    mario.floorHeight = find_floor(0, 0, 0, &mario.floor);
    mario.pos[1] = 100;
    CHECK(perform_air_step(&mario, 0) == AIR_STEP_NONE);
    CHECK(mario.pos[1] == 100 && mario.vel[1] == -4);
    CHECK(perform_air_step(&mario, 0) == AIR_STEP_NONE);
    CHECK(mario.pos[1] == 96 && mario.vel[1] == -8);
    int result = AIR_STEP_NONE;
    for (int i = 0; i < 20 && result != AIR_STEP_LANDED; ++i) result = perform_air_step(&mario, 0);
    CHECK(result == AIR_STEP_LANDED && mario.pos[1] == 0);
    CHECK(object.header.gfx.pos[1] == 0);
    sm64_terrain_destroy(terrain);

    terrain = sm64_terrain_create(surfaces, 2, NULL, 0);
    CHECK(terrain != NULL);
    sm64_terrain_activate(terrain);
    mario.floorHeight = find_floor(0, 0, 0, &mario.floor);
    mario.vel[1] = 40;
    mario.action = ACT_JUMP;
    CHECK(perform_air_step(&mario, 0) == AIR_STEP_NONE);
    CHECK(mario.pos[1] == 40 && mario.vel[1] == 36);
    mario.ceilHeight = find_ceil(0, 80, 0, &mario.ceil);
    CHECK(perform_air_step(&mario, AIR_STEP_CHECK_HANG) == AIR_STEP_GRABBED_CEILING);
    CHECK(mario.pos[1] == 40 && mario.vel[1] == -4);
    mario.vel[1] = 36;
    CHECK(perform_air_step(&mario, 0) == AIR_STEP_NONE);
    CHECK(mario.pos[1] == 40 && mario.vel[1] == -4);
    sm64_terrain_destroy(terrain);

    surfaces[1] = surfaces[2];
    terrain = sm64_terrain_create(surfaces, 2, NULL, 0);
    CHECK(terrain != NULL);
    sm64_terrain_activate(terrain);
    mario.floorHeight = find_floor(0, 100, 0, &mario.floor);
    mario.pos[1] = 100;
    mario.vel[1] = 0;
    mario.vel[0] = 64;
    mario.faceAngle[1] = 0x4000;
    CHECK(perform_air_step(&mario, 0) == AIR_STEP_HIT_LAVA_WALL);
    CHECK(mario.wall != NULL && mario.wall->type == SURFACE_BURNING && mario.pos[0] == 50);
    sm64_terrain_destroy(terrain);

    surfaces[1] = (struct sm64_terrain_triangle){
        .vertices = {{100,0,-1000}, {100,0,1000}, {100,120,0}}, .type = SURFACE_DEFAULT};
    surfaces[2] = (struct sm64_terrain_triangle){
        .vertices = {{100,120,-1000}, {100,120,1000}, {500,120,0}}, .type = SURFACE_DEFAULT};
    terrain = sm64_terrain_create(surfaces, 3, NULL, 0);
    CHECK(terrain != NULL);
    sm64_terrain_activate(terrain);
    mario.pos[0] = 0;
    mario.pos[1] = 10;
    mario.vel[0] = 64;
    mario.vel[1] = -1;
    mario.floorHeight = find_floor(0, 10, 0, &mario.floor);
    mario.ceil = NULL;
    CHECK(perform_air_step(&mario, AIR_STEP_CHECK_LEDGE_GRAB) == AIR_STEP_GRABBED_LEDGE);
    CHECK(mario.pos[0] == 110 && mario.pos[1] == 120 && mario.floorHeight == 120);
    sm64_terrain_destroy(terrain);
    puts("Original air stepping: falling, landing, ceiling/ledge grabs and lava walls passed");
    return 0;
}
