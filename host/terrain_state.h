#ifndef SM64_PHYSICS_TERRAIN_STATE_H
#define SM64_PHYSICS_TERRAIN_STATE_H
#include "sm64.h"
#include "engine/surface_load.h"
#include "game/object_list_processor.h"
#include "game/mario.h"
#include "game/object_helpers.h"
#include "game/memory.h"
#include "behavior_data.h"
#include "terrain.h"

struct sm64_terrain {
    SpatialPartitionCell static_partition[NUM_CELLS][NUM_CELLS];
    SpatialPartitionCell dynamic_partition[NUM_CELLS][NUM_CELLS];
    struct SurfaceNode *nodes;
    struct Surface *surfaces;
    s32 node_count, surface_count;
    s32 static_node_count, static_surface_count;
    u32 time_stop;
    const BehaviorScript *ddd_warp_behavior;
    s16 surface_capacity;
    struct NumTimesCalled calls;
    s32 floor_misses;
    s16 camera, include_intangible;
    struct Object *current_object, *mario_object;
    struct MarioState *mario;
    struct FloorGeometry floor_geometry;
    TerrainData *environment;
    struct sm64_terrain_triangle *triangles;
    size_t triangle_count;
    struct sm64_terrain_region *regions;
    size_t region_count;
};
extern _Thread_local struct sm64_terrain *sm64_active_terrain;

#ifdef SM64_PHYSICS_TERRAIN_IMPLEMENTATION
#define gStaticSurfacePartition (sm64_active_terrain->static_partition)
#define gDynamicSurfacePartition (sm64_active_terrain->dynamic_partition)
#define sSurfaceNodePool (sm64_active_terrain->nodes)
#define sSurfacePool (sm64_active_terrain->surfaces)
#define sSurfacePoolSize (sm64_active_terrain->surface_capacity)
#define gSurfaceNodesAllocated (sm64_active_terrain->node_count)
#define gSurfacesAllocated (sm64_active_terrain->surface_count)
#define gCheckingSurfaceCollisionsForCamera (sm64_active_terrain->camera)
#define gFindFloorIncludeSurfaceIntangible (sm64_active_terrain->include_intangible)
#define gCurrentObject (sm64_active_terrain->current_object)
#define gMarioObject (sm64_active_terrain->mario_object)
#define gMarioState (sm64_active_terrain->mario)
#define gNumCalls (sm64_active_terrain->calls)
#define gNumFindFloorMisses (sm64_active_terrain->floor_misses)
#define gEnvironmentRegions (sm64_active_terrain->environment)
#define sFloorGeo (sm64_active_terrain->floor_geometry)
#define gTimeStopState (sm64_active_terrain->time_stop)
#define gNumStaticSurfaces (sm64_active_terrain->static_surface_count)
#define gNumStaticSurfaceNodes (sm64_active_terrain->static_node_count)
#define bhvDDDWarp (sm64_active_terrain->ddd_warp_behavior)
#endif
#endif
