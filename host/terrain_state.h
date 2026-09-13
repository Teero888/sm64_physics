#ifndef SM64_PHYSICS_TERRAIN_STATE_H
#define SM64_PHYSICS_TERRAIN_STATE_H
#include "sm64.h"
#include "engine/surface_load.h"
#include "game/object_list_processor.h"
#include "game/mario.h"
#include "game/mario_step.h"
#include "game/area.h"
#include "game/level_update.h"
#include "game/camera.h"
#include "game/interaction.h"
#include "host/mario_motion.h"
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
    s16 level_num;
    s16 camera_movement_flags;
    struct Surface water_floor;
    u32 time_stop;
    const BehaviorScript *ddd_warp_behavior;
    s16 surface_capacity;
    struct NumTimesCalled calls;
    s32 floor_misses;
    s16 camera, include_intangible;
    struct Object *current_object, *mario_object;
    struct MarioState *mario;
    /* Optional live bindings while an owned object frame is executing. */
    struct Object **current_ref, **mario_object_ref;
    struct MarioState **mario_ref;
    u32 *time_stop_ref;
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
#define gCurrentObject (*(sm64_active_terrain->current_ref ? sm64_active_terrain->current_ref : &sm64_active_terrain->current_object))
#define gMarioObject (*(sm64_active_terrain->mario_object_ref ? sm64_active_terrain->mario_object_ref : &sm64_active_terrain->mario_object))
#define gMarioState (*(sm64_active_terrain->mario_ref ? sm64_active_terrain->mario_ref : &sm64_active_terrain->mario))
#define gNumCalls (sm64_active_terrain->calls)
#define gNumFindFloorMisses (sm64_active_terrain->floor_misses)
#define gEnvironmentRegions (sm64_active_terrain->environment)
#define sFloorGeo (sm64_active_terrain->floor_geometry)
#define gTimeStopState (*(sm64_active_terrain->time_stop_ref ? sm64_active_terrain->time_stop_ref : &sm64_active_terrain->time_stop))
#define gNumStaticSurfaces (sm64_active_terrain->static_surface_count)
#define gNumStaticSurfaceNodes (sm64_active_terrain->static_node_count)
#define bhvDDDWarp (sm64_active_terrain->ddd_warp_behavior)
#define gCurrLevelNum (sm64_active_terrain->level_num)
#define gWaterSurfacePseudoFloor (sm64_active_terrain->water_floor)
#define gCameraMovementFlags (sm64_active_terrain->camera_movement_flags)
#endif
#endif
