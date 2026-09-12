#include <stdio.h>
#include <stdlib.h>
#include "sm64.h"
#include "../host/terrain.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

int main(void) {
    struct sm64_terrain_triangle triangles[] = {
        {.vertices = {{-1000,0,-1000}, {0,0,1000}, {1000,0,-1000}}, .type = SURFACE_DEFAULT},
        {.vertices = {{-1000,100,-1000}, {0,100,1000}, {1000,100,-1000}},
         .type = SURFACE_NO_CAM_COLLISION, .dynamic = true},
        {.vertices = {{-1000,500,-1000}, {1000,500,-1000}, {0,500,1000}}, .type = SURFACE_DEFAULT},
        {.vertices = {{0,0,-1000}, {0,1000,0}, {0,0,1000}}, .type = SURFACE_DEFAULT},
    };
    struct sm64_terrain_region regions[] = {
        {0, -100, -100, 100, 100, 300}, {50, -100, -100, 100, 100, 200}
    };
    struct sm64_terrain *a = sm64_terrain_create(triangles, 4, regions, 2);
    CHECK(a != NULL && sm64_terrain_surface_count(a) == 4);
    CHECK(sm64_terrain_activate(a) == NULL);
    struct Surface *hit = NULL;
    CHECK(find_floor(0, 0, 0, &hit) == 0 && hit != NULL && hit->normal.y == 1);
    CHECK(find_floor(900, 200, 900, &hit) == FLOOR_LOWER_LIMIT && hit == NULL);
    CHECK(find_floor(8192, 200, 0, &hit) == FLOOR_LOWER_LIMIT && hit == NULL);
    CHECK(find_floor(0, -78, 0, &hit) == 0 && hit != NULL);
    CHECK(find_floor(0, -79, 0, &hit) == FLOOR_LOWER_LIMIT && hit == NULL);
    CHECK(find_floor(0, 200, 0, &hit) == 100 && (hit->flags & SURFACE_FLAG_DYNAMIC));
    sm64_terrain_set_query_state(a, true, NULL, NULL, NULL);
    CHECK(find_floor(0, 200, 0, &hit) == 0);
    sm64_terrain_set_query_state(a, false, NULL, NULL, NULL);
    CHECK(find_floor(0, 200, 0, &hit) == 100);
    CHECK(find_ceil(0, 200, 0, &hit) == 500 && hit->normal.y == -1);
    struct WallCollisionData wall = {.x = 10, .y = 100, .z = 0, .radius = 20};
    CHECK(find_wall_collisions(&wall) == 1 && wall.numWalls == 1 && wall.x == 20);
    CHECK(find_water_level(0, 0) == 300 && find_poison_gas_level(0, 0) == 200);
    CHECK(find_water_level(100, 0) == FLOOR_LOWER_LIMIT);
    CHECK(find_poison_gas_level(0, 100) == FLOOR_LOWER_LIMIT);
    struct sm64_terrain *copy = sm64_terrain_clone(a);
    CHECK(copy != NULL);
    for (int i = 0; i < 3; ++i) triangles[0].vertices[i][1] = 50;
    struct sm64_terrain *b = sm64_terrain_create(triangles, 1, NULL, 0);
    CHECK(b != NULL);
    for (int i = 0; i < 30; ++i) {
        sm64_terrain_activate(b);
        CHECK(find_floor(0, 200, 0, &hit) == 50);
        sm64_terrain_activate(a);
        CHECK(find_floor(0, 200, 0, &hit) == 100);
    }
    sm64_terrain_destroy(a);
    CHECK(sm64_terrain_activate(copy) == NULL);
    CHECK(find_floor(0, 200, 0, &hit) == 100);
    CHECK(find_water_level(0, 0) == 300);
    sm64_terrain_destroy(b);
    CHECK(find_floor(0, 0, 0, &hit) == 0);
    sm64_terrain_destroy(copy);
    struct sm64_terrain_triangle degenerate = {0};
    struct sm64_terrain *empty = sm64_terrain_create(&degenerate, 1, NULL, 0);
    CHECK(empty != NULL && sm64_terrain_surface_count(empty) == 0);
    sm64_terrain_activate(empty);
    CHECK(find_floor(0, 0, 0, &hit) == FLOOR_LOWER_LIMIT && hit == NULL);
    sm64_terrain_destroy(empty);
    CHECK(sm64_terrain_create(NULL, 1, NULL, 0) == NULL);
    CHECK(sm64_terrain_create(triangles, 2301, NULL, 0) == NULL);
    CHECK(sm64_terrain_create(NULL, 0, NULL, 1) == NULL);
    struct sm64_terrain_triangle wide[28];
    for (int i = 0; i < 28; ++i) {
        wide[i] = (struct sm64_terrain_triangle){
            .vertices = {{-8000,0,-8000}, {0,0,8000}, {8000,0,-8000}}, .type = SURFACE_DEFAULT
        };
    }
    CHECK(sm64_terrain_create(wide, 28, NULL, 0) == NULL); /* 7168 partition nodes exceed 7000. */
    puts("Owned terrain: original loading/queries, walls, independent instances and clone lifetime passed");
    return 0;
}
