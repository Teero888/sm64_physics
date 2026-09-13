#include <assert.h>
#include <stdio.h>
#include "../host/behavior.h"
#include "../host/audio.h"
#include "../host/terrain.h"
#include "game/object_list_processor.h"
#include "engine/graph_node.h"

u16 gAreaUpdateCounter;
static unsigned calls;
static void native_callback(void) { ++calls; }
/* Native bytecode words using upstream's command encoding. Field 0x3f is
 * oHealth. Pointers occupy native words rather than ROM segmented addresses. */
#define OP(code) ((uintptr_t)(code) << 24)
#define FIELD(code, field, value) (OP(code) | ((field) << 16) | (u16)(value))
static const BehaviorScript subroutine[] = {FIELD(0x0f, 0x3f, 2), OP(0x03)};
static const BehaviorScript child_script[] = {OBJ_LIST_DEFAULT << 16, FIELD(0x10, 0x3f, 4), OP(0x0a)};
static const BehaviorScript script[] = {
    OBJ_LIST_DEFAULT << 16,
    FIELD(0x10, 0x3f, 7),
    OP(0x02), (uintptr_t)subroutine,
    OP(0x01) | 2,
    OP(0x1c), 0, (uintptr_t)child_script,
    OP(0x0c), (uintptr_t)native_callback,
    OP(0x08), FIELD(0x0f, 0x3f, 1), OP(0x09)
};

int main(void) {
    struct sm64_objects *a = sm64_objects_create(), *b = sm64_objects_create();
    struct sm64_audio_state *audio = sm64_audio_create(NULL, NULL);
    struct sm64_terrain *terrain = sm64_terrain_create(NULL, 0, NULL, 0);
    assert(a && b && audio && terrain);
    struct GraphNode model = {0};
    struct GraphNode *models[] = {&model};
    sm64_objects_bind_behavior_assets(a, models, NULL, NULL, script);
    sm64_objects_bind_behavior_assets(b, models, NULL, NULL, NULL);
    sm64_audio_activate(audio);
    sm64_terrain_activate(terrain);
    sm64_objects_activate(a);
    struct Object *parent = sm64_objects_spawn(script);
    assert(parent);
    parent->oFlags = OBJ_FLAG_MOVE_XZ_USING_FVEL | OBJ_FLAG_UPDATE_GFX_POS_AND_ANGLE;
    parent->oForwardVel = 2;
    parent->oMoveAngleYaw = 0x4000;
    parent->header.gfx.areaIndex = 1;
    sm64_objects_update_behavior(parent, 0);
    assert(parent->oHealth == 9 && parent->bhvDelayTimer == 1 && parent->bhvStackIndex == 0);
    assert(parent->oTimer == 1 && parent->oPosX == 2 && parent->header.gfx.pos[0] == 2);
    assert(parent->oCollisionDistance == 150);
    sm64_objects_update_behavior(parent, 1);
    assert(parent->oHealth == 9 && parent->bhvDelayTimer == 0 && calls == 0);
    sm64_objects_update_behavior(parent, 2);
    assert(parent->oHealth == 10 && parent->oPosX == 6 && parent->bhvStackIndex == 1);
    assert(calls == 1);
    struct Object *child = (struct Object *)parent->header.next;
    assert(child->behavior == child_script && child->parentObj == parent);
    assert(child->oPosX == 4 && child->header.gfx.sharedChild == &model);
    assert(child->header.gfx.areaIndex == 1 && child->oHealth == 2048);
    sm64_objects_update_behavior(child, 2);
    assert(child->oHealth == 4);
    sm64_objects_update_behavior(parent, 3);
    assert(parent->oHealth == 11 && calls == 1);
    parent->oAction = 5;
    parent->oSubAction = 3;
    sm64_objects_update_behavior(parent, 4);
    assert(parent->oTimer == 1 && parent->oSubAction == 0 && parent->oPrevAction == 5);

    sm64_objects_activate(b);
    struct Object *other = sm64_objects_spawn(script);
    sm64_objects_update_behavior(other, 100);
    assert(other->oHealth == 9 && other->oCollisionDistance == 1000);
    assert(parent->oHealth == 12);
    sm64_objects_destroy(a);
    sm64_objects_update_behavior(other, 101);
    sm64_objects_update_behavior(other, 102);
    assert(other->oHealth == 10 && calls == 2);
    sm64_objects_destroy(b);
    sm64_terrain_destroy(terrain);
    sm64_audio_destroy(audio);
    puts("Original behavior execution: calls, delays, loops, spawning, movement, action timers and independent contexts passed");
    return 0;
}
