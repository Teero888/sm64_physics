#include <stdio.h>
#include <stdlib.h>
#include "sm64.h"
#include "engine/surface_load.h"
#include "game/object_list_processor.h"
#include "../host/terrain.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

int main(void) {
    struct sm64_terrain_triangle floor = {
        .vertices = {{-1000,0,-1000}, {0,0,1000}, {1000,0,-1000}}, .type = SURFACE_DEFAULT
    };
    struct sm64_terrain *terrain = sm64_terrain_create(&floor, 1, NULL, 0);
    CHECK(terrain != NULL);
    sm64_terrain_activate(terrain);
    TerrainData collision[] = {
        TERRAIN_LOAD_VERTICES, 3, -100,0,-100, 0,0,100, 100,0,-100,
        SURFACE_DEFAULT, 1, 0,1,2, TERRAIN_LOAD_CONTINUE
    };
    const BehaviorScript normal_behavior[] = {0}, warp_behavior[] = {0};
    struct Object object = {0}, mario = {0};
    object.behavior = normal_behavior;
    object.collisionData = collision;
    object.oPosY = 100;
    object.oCollisionDistance = 1000;
    object.oDrawingDistance = 4000;
    object.header.gfx.scale[0] = object.header.gfx.scale[1] = object.header.gfx.scale[2] = 1;
    sm64_terrain_set_query_state(terrain, false, NULL, &mario, NULL);
    sm64_terrain_set_ddd_warp_behavior(terrain, warp_behavior);
    CHECK(sm64_terrain_load_object(terrain, &object, sizeof(collision)/sizeof(*collision)));
    struct Surface *hit;
    CHECK(find_floor(0, 500, 0, &hit) == 100 && hit->object == &object && hit->room == 0);
    CHECK(sm64_terrain_surface_count(terrain) == 2);
    struct sm64_terrain *copy = sm64_terrain_clone(terrain);
    CHECK(copy != NULL);
    clear_dynamic_surfaces();
    CHECK(find_floor(0, 500, 0, &hit) == 0);
    object.oPosY = 250;
    object.header.gfx.throwMatrix = NULL;
    CHECK(sm64_terrain_load_object(terrain, &object, sizeof(collision)/sizeof(*collision)));
    CHECK(find_floor(0, 500, 0, &hit) == 250);
    sm64_terrain_set_time_stop(terrain, TIME_STOP_ACTIVE);
    clear_dynamic_surfaces();
    CHECK(find_floor(0, 500, 0, &hit) == 250);
    sm64_terrain_set_time_stop(terrain, 0);
    clear_dynamic_surfaces();
    object.oDistanceToMario = 2000;
    CHECK(sm64_terrain_load_object(terrain, &object, sizeof(collision)/sizeof(*collision)));
    CHECK(find_floor(0, 500, 0, &hit) == 0);
    object.oDistanceToMario = 0;
    object.behavior = warp_behavior;
    CHECK(sm64_terrain_load_object(terrain, &object, sizeof(collision)/sizeof(*collision)));
    CHECK(find_floor(0, 500, 0, &hit) == 250 && hit->room == 5);
    size_t count = sm64_terrain_surface_count(terrain);
    CHECK(!sm64_terrain_load_object(terrain, &object, 5));
    collision[15] = 3; /* Out-of-range vertex index. */
    CHECK(!sm64_terrain_load_object(terrain, &object, sizeof(collision)/sizeof(*collision)));
    CHECK(sm64_terrain_surface_count(terrain) == count);
    sm64_terrain_destroy(terrain);
    sm64_terrain_activate(copy);
    CHECK(find_floor(0, 500, 0, &hit) == 100 && hit->object == &object);
    clear_dynamic_surfaces();
    CHECK(find_floor(0, 500, 0, &hit) == 0);
    sm64_terrain_destroy(copy);
    puts("Moving surfaces: original object loading, transforms, culling, time stop and copy lifetime passed");
    return 0;
}
