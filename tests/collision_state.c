/* Test-owned query context, never linked into the production library. */
#include "sm64.h"
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

void print_debug_top_down_mapinfo(const char *text, s32 value) { (void) text; (void) value; }
void set_text_array_x_y(s32 x, s32 y) { (void) x; (void) y; }
