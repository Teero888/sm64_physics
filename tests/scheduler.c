#include <assert.h>
#include <stdio.h>
#include "../host/scheduler.h"
#include "../host/audio.h"
#include "game/object_list_processor.h"
#include "game/interaction.h"
#include "engine/graph_node.h"
static int calls[32], count;
static void surface_call(void) { calls[count++] = 1; }
static void regular_call(void) { calls[count++] = 2; }
static void small_call(void) { calls[count++] = 3; }
#define SCRIPT(list, fn) { (list) << 16, 0x08000000, 0x0c000000, (uintptr_t)(fn), 0x09000000 }
static const BehaviorScript surface[] = SCRIPT(OBJ_LIST_SURFACE, surface_call);
static const BehaviorScript regular[] = SCRIPT(OBJ_LIST_DEFAULT, regular_call);
static const BehaviorScript small[] = SCRIPT(OBJ_LIST_UNIMPORTANT, small_call);
int main(void) {
    struct sm64_objects *objects = sm64_objects_create();
    struct sm64_audio_state *audio = sm64_audio_create(NULL, NULL);
    assert(objects && audio);
    sm64_objects_activate(objects);
    sm64_audio_activate(audio);
    struct Object *a = sm64_objects_spawn(regular);
    struct Object *b = sm64_objects_spawn(surface);
    struct Object *c = sm64_objects_spawn(small);
    assert(a && b && c);
    update_terrain_objects();
    update_non_terrain_objects();
    assert(count == 3 && calls[0] == 1 && calls[1] == 2 && calls[2] == 3);
    count = 0;
    sm64_objects_set_motion_state(objects, NULL, NULL, TIME_STOP_ACTIVE);
    update_terrain_objects();
    update_non_terrain_objects();
    assert(count == 1 && calls[0] == 3);
    assert(!(a->header.gfx.node.flags & GRAPH_RENDER_HAS_ANIMATION));
    assert(c->header.gfx.node.flags & GRAPH_RENDER_HAS_ANIMATION);
    sm64_objects_set_mario(objects, a);
    b->oInteractType = INTERACT_DOOR;
    count = 0;
    update_terrain_objects();
    update_non_terrain_objects();
    assert(count == 3);
    count = 0;
    sm64_objects_set_motion_state(objects, NULL, NULL, TIME_STOP_ACTIVE | TIME_STOP_MARIO_AND_DOORS);
    update_terrain_objects();
    update_non_terrain_objects();
    assert(count == 1 && calls[0] == 3);
    b->activeFlags |= ACTIVE_FLAG_INITIATED_TIME_STOP;
    count = 0;
    update_terrain_objects();
    update_non_terrain_objects();
    assert(count == 2 && calls[0] == 1 && calls[1] == 3);
    count = 0;
    sm64_objects_set_motion_state(objects, NULL, NULL, TIME_STOP_ACTIVE | TIME_STOP_ALL_OBJECTS);
    update_terrain_objects();
    update_non_terrain_objects();
    assert(count == 0);
    u32 respawn32 = 0x12340001;
    u16 respawn16 = 0x0042;
    a->respawnInfoType = RESPAWN_INFO_TYPE_32;
    a->respawnInfo = &respawn32;
    b->respawnInfoType = RESPAWN_INFO_TYPE_16;
    b->respawnInfo = &respawn16;
    mark_obj_for_deletion(a);
    mark_obj_for_deletion(b);
    unload_deactivated_objects();
    assert(respawn32 == 0x1234ff01 && respawn16 == 0xff42);
    assert(sm64_objects_list(objects, OBJ_LIST_DEFAULT)->next == sm64_objects_list(objects, OBJ_LIST_DEFAULT));
    c->respawnInfoType = RESPAWN_INFO_TYPE_16;
    respawn16 = 0x42;
    c->respawnInfo = &respawn16;
    c->oFlags |= OBJ_FLAG_PERSISTENT_RESPAWN;
    mark_obj_for_deletion(c);
    unload_deactivated_objects();
    assert(respawn16 == 0x42);
    sm64_objects_destroy(objects);
    sm64_audio_destroy(audio);
    puts("Original object scheduling: phase order, selective time stop and respawn-aware deletion passed");
    return 0;
}
