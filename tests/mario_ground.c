#include <stdio.h>
#include <stdlib.h>
#include "sm64.h"
#include "game/area.h"
#include "game/mario_step.h"
#include "../host/terrain.h"
#include "../host/mario_motion.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

int main(void) {
    struct sm64_terrain_triangle triangles[] = {
        {.vertices = {{-1000,0,-1000}, {0,0,1000}, {1000,0,-1000}}, .type = SURFACE_DEFAULT},
        {.vertices = {{100,0,-1000}, {100,0,1000}, {100,1000,0}}, .type = SURFACE_DEFAULT}
    };
    struct sm64_terrain *terrain = sm64_terrain_create(triangles, 2, NULL, 0);
    CHECK(terrain != NULL);
    sm64_terrain_activate(terrain);
    struct Object object = {0};
    struct Area area = {.terrainType = TERRAIN_GRASS};
    struct MarioBodyState body = {0};
    struct MarioState mario = {.marioObj = &object, .area = &area,
        .marioBodyState = &body, .action = ACT_WALKING, .waterLevel = -11000};
    mario.floorHeight = find_floor(0, 0, 0, &mario.floor);
    mario.vel[0] = 8;
    mario.faceAngle[1] = 0x4000;
    for (int i = 0; i < 5; ++i) {
        CHECK(perform_ground_step(&mario) == GROUND_STEP_NONE);
        CHECK(mario.pos[0] == 8 * (i + 1) && mario.pos[1] == 0);
        CHECK(object.header.gfx.pos[0] == mario.pos[0]);
    }
    int hit_wall = 0;
    for (int i = 0; i < 10; ++i) {
        hit_wall |= perform_ground_step(&mario) == GROUND_STEP_HIT_WALL;
    }
    CHECK(hit_wall && mario.pos[0] == 50 && mario.wall != NULL);

    mario.action = ACT_JUMP;
    mario.flags = 0;
    mario.vel[1] = 40;
    apply_gravity(&mario);
    CHECK(mario.vel[1] == 36);
    mario.flags = MARIO_UNKNOWN_08;
    mario.input = 0;
    apply_gravity(&mario);
    CHECK(mario.vel[1] == 9); /* Releasing A cuts jump ascent. */
    mario.flags = 0;
    for (int i = 0; i < 100; ++i) apply_gravity(&mario);
    CHECK(mario.vel[1] == -75);
    mario.action = ACT_LONG_JUMP;
    mario.vel[1] = 30;
    apply_gravity(&mario);
    CHECK(mario.vel[1] == 28);
    mario.action = ACT_TWIRLING;
    mario.angleVel[1] = 2048;
    mario.vel[1] = -70;
    apply_gravity(&mario);
    CHECK(mario.vel[1] == -37.5f);
    sm64_terrain_destroy(terrain);

    struct sm64_terrain_region water = {0, -500, -500, 500, 500, 100};
    terrain = sm64_terrain_create(triangles, 1, &water, 1);
    CHECK(terrain != NULL);
    sm64_terrain_activate(terrain);
    mario.pos[0] = mario.pos[2] = 0;
    mario.pos[1] = 100;
    mario.vel[0] = 8;
    mario.floorHeight = find_floor(0, 100, 0, &mario.floor);
    mario.action = ACT_RIDING_SHELL_GROUND;
    CHECK(perform_ground_step(&mario) == GROUND_STEP_NONE);
    CHECK(mario.pos[1] == 100 && mario.floorHeight == 100);
    CHECK(mario.floor->type == SURFACE_VERY_SLIPPERY);
    sm64_terrain_destroy(terrain);
    puts("Original Mario ground stepping: movement, walls, gravity and shell water surface passed");
    return 0;
}
