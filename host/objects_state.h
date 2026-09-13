#ifndef SM64_PHYSICS_OBJECTS_STATE_H
#define SM64_PHYSICS_OBJECTS_STATE_H
#include "host/objects.h"
#include "audio/external.h"
#include "engine/graph_node.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "game/object_helpers.h"
#include "game/object_list_processor.h"
#include "level_table.h"
struct sm64_objects {
    struct Object pool[OBJECT_POOL_CAPACITY];
    struct ObjectNode lists[NUM_OBJ_LISTS], free_list;
    struct ObjectNode *active_lists;
    struct GraphNode parent;
    struct Object *current, *mario;
    struct Object *mario_platform;
    struct MarioState *mario_state;
    u32 time_stop;
    u16 displacement_state;
    u16 random_seed;
    u32 global_timer;
    u32 object_counter;
    u32 previous_object_count;
    u8 debug_level_select;
    Vec3f global_sound_source;
    const BehaviorScript *command;
    struct GraphNode **models;
    const BehaviorScript *haunted_chair, *mad_piano, *message_panel;
    RoomData adjacent_rooms[60][2];
    s16 mario_room, roomed_near, roomed_far;
    s16 level;
};
extern _Thread_local struct sm64_objects *sm64_active_objects;
#define gObjectPool (sm64_active_objects->pool)
#define gObjectLists (sm64_active_objects->active_lists)
#define gFreeObjectList (sm64_active_objects->free_list)
#define gObjParentGraphNode (sm64_active_objects->parent)
#define gCurrentObject (sm64_active_objects->current)
#define gMarioObject (sm64_active_objects->mario)
#define gMarioPlatform (sm64_active_objects->mario_platform)
#define gMarioStates (sm64_active_objects->mario_state)
#define gTimeStopState (sm64_active_objects->time_stop)
#define D_8032FEC0 (sm64_active_objects->displacement_state)
#define gCurrLevelNum (sm64_active_objects->level)
#endif
