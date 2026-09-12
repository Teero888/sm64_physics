/* Exercise the original collision queries with explicitly owned test state.
 * This does not claim to exercise the terrain loader or Mario simulation. */
#include <stdio.h>
#include <stdlib.h>
#include "sm64.h"
#include "engine/surface_collision.h"
#include "engine/surface_load.h"
#include "game/object_list_processor.h"
#include "game/mario.h"
#include "game/debug.h"

SpatialPartitionCell gStaticSurfacePartition[NUM_CELLS][NUM_CELLS];
SpatialPartitionCell gDynamicSurfacePartition[NUM_CELLS][NUM_CELLS];
struct Object *gCurrentObject, *gMarioObject;
struct MarioState *gMarioState;
struct NumTimesCalled gNumCalls;
s16 gCheckingSurfaceCollisionsForCamera, gFindFloorIncludeSurfaceIntangible;
s32 gNumFindFloorMisses, gNumStaticSurfaces, gSurfaceNodesAllocated, gSurfacesAllocated;
TerrainData *gEnvironmentRegions;

/* Only the unused debug overlay depends on these presentation hooks. */
void print_debug_top_down_mapinfo(const char *text, s32 value) { (void) text; (void) value; }
void set_text_array_x_y(s32 x, s32 y) { (void) x; (void) y; }

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

int main(void) {
    struct Surface floor = {
        .type = SURFACE_DEFAULT, .lowerY = -5, .upperY = 5,
        .vertex1 = {-1000, 0, -1000}, .vertex2 = {0, 0, 1000},
        .vertex3 = {1000, 0, -1000}, .normal = {0, 1, 0}
    };
    struct Surface platform = floor;
    platform.originOffset = -100;
    platform.lowerY = 95;
    platform.upperY = 105;
    platform.vertex1[1] = platform.vertex2[1] = platform.vertex3[1] = 100;
    struct SurfaceNode floor_node = {.surface = &floor};
    struct SurfaceNode platform_node = {.surface = &platform};
    struct Surface *hit = NULL;
    gStaticSurfacePartition[8][8][SPATIAL_PARTITION_FLOORS].next = &floor_node;
    CHECK(find_floor(0, 200, 0, &hit) == 0 && hit == &floor);
    CHECK(find_floor(900, 200, 900, &hit) == FLOOR_LOWER_LIMIT && hit == NULL);
    CHECK(find_floor(LEVEL_BOUNDARY_MAX, 200, 0, &hit) == FLOOR_LOWER_LIMIT && hit == NULL);
    /* The original collision routine permits floors 78 units above Mario. */
    CHECK(find_floor(0, -78, 0, &hit) == 0 && hit == &floor);
    CHECK(find_floor(0, -79, 0, &hit) == FLOOR_LOWER_LIMIT && hit == NULL);
    gDynamicSurfacePartition[8][8][SPATIAL_PARTITION_FLOORS].next = &platform_node;
    CHECK(find_floor(0, 200, 0, &hit) == 100 && hit == &platform);
    CHECK(find_floor(0, 0, 0, &hit) == 0 && hit == &floor);
    platform.flags = SURFACE_FLAG_NO_CAM_COLLISION;
    gCheckingSurfaceCollisionsForCamera = 1;
    CHECK(find_floor(0, 200, 0, &hit) == 0 && hit == &floor);
    gCheckingSurfaceCollisionsForCamera = 0;
    CHECK(find_floor(0, 200, 0, &hit) == 100 && hit == &platform);

    struct Surface ceiling = {
        .type = SURFACE_DEFAULT, .lowerY = 495, .upperY = 505,
        .vertex1 = {-1000, 500, -1000}, .vertex2 = {1000, 500, -1000},
        .vertex3 = {0, 500, 1000}, .normal = {0, -1, 0}, .originOffset = 500
    };
    struct SurfaceNode ceiling_node = {.surface = &ceiling};
    gStaticSurfacePartition[8][8][SPATIAL_PARTITION_CEILS].next = &ceiling_node;
    CHECK(find_ceil(0, 200, 0, &hit) == 500 && hit == &ceiling);

    TerrainData environment[] = {2, 0, -100, -100, 100, 100, 300,
                                   50, -100, -100, 100, 100, 200};
    gEnvironmentRegions = environment;
    CHECK(find_water_level(0, 0) == 300);
    CHECK(find_poison_gas_level(0, 0) == 200);
    CHECK(find_water_level(100, 0) == FLOOR_LOWER_LIMIT);
    CHECK(find_poison_gas_level(0, 100) == FLOOR_LOWER_LIMIT);
    gEnvironmentRegions = NULL;
    CHECK(find_water_level(0, 0) == FLOOR_LOWER_LIMIT);
    puts("Native collision: floors, platforms, ceilings, camera filtering, water and gas passed");
    return 0;
}
