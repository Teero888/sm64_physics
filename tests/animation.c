#include <stdio.h>
#include <stdlib.h>
#include "sm64.h"
#include "game/area.h"
#include "game/memory.h"
#include "../host/animation.h"

static struct sm64_objects *world;

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

static void tick(struct GraphNodeObject *object) {
    sm64_objects_set_animation_tick(world, sm64_objects_animation_tick(world) + 1);
    sm64_physics_advance_object_animation(object);
}

int main(void) {
    world = sm64_objects_create();
    CHECK(world);
    sm64_objects_activate(world);
    struct Animation animation = {.startFrame = 2, .loopStart = 2, .loopEnd = 5};
    struct Animation *animation_ptr = &animation;
    struct GraphNodeObject object = {0};
    object.node.flags = GRAPH_RENDER_HAS_ANIMATION;
    geo_obj_init_animation(&object, &animation_ptr);
    CHECK(object.animInfo.curAnim == &animation);
    CHECK(object.animInfo.animFrame == 1);
    tick(&object);
    CHECK(object.animInfo.animFrame == 2);
    sm64_physics_advance_object_animation(&object);
    CHECK(object.animInfo.animFrame == 2); /* Same simulation tick is idempotent. */
    geo_obj_init_animation(&object, &animation_ptr);
    CHECK(object.animInfo.animFrame == 2); /* Same animation does not restart. */
    tick(&object);
    CHECK(object.animInfo.animFrame == 3);
    tick(&object);
    CHECK(object.animInfo.animFrame == 4);
    tick(&object);
    CHECK(object.animInfo.animFrame == 2);

    animation.flags = ANIM_FLAG_NOLOOP;
    for (int i = 0; i < 10; ++i) tick(&object);
    CHECK(object.animInfo.animFrame == 4);
    animation.flags = ANIM_FLAG_BACKWARD;
    tick(&object);
    CHECK(object.animInfo.animFrame == 3);
    tick(&object);
    CHECK(object.animInfo.animFrame == 2);
    tick(&object);
    CHECK(object.animInfo.animFrame == 4);
    animation.flags |= ANIM_FLAG_NOLOOP;
    for (int i = 0; i < 10; ++i) tick(&object);
    CHECK(object.animInfo.animFrame == 2);
    animation.flags = ANIM_FLAG_2;
    tick(&object);
    CHECK(object.animInfo.animFrame == 2);

    animation.flags = 0;
    object.animInfo.curAnim = NULL;
    geo_obj_init_animation_accel(&object, &animation_ptr, 0x8000);
    CHECK(object.animInfo.animFrameAccelAssist == 0x18000);
    tick(&object);
    CHECK(object.animInfo.animFrameAccelAssist == 0x20000);
    tick(&object);
    CHECK(object.animInfo.animFrame == 2 && object.animInfo.animFrameAccelAssist == 0x28000);
    tick(&object);
    CHECK(object.animInfo.animFrame == 3);
    object.node.flags = 0;
    tick(&object);
    CHECK(object.animInfo.animFrame == 3);
    CHECK(object.animInfo.animTimer == sm64_objects_animation_tick(world));
    object.node.flags = GRAPH_RENDER_HAS_ANIMATION;
    sm64_objects_set_animation_tick(world, 0xffff);
    object.animInfo.animTimer = 0xffff;
    tick(&object);
    CHECK(sm64_objects_animation_tick(world) == 0 && object.animInfo.animFrameAccelAssist == 0x38000);

    struct sm64_objects *other = sm64_objects_create();
    CHECK(other);
    struct GraphNodeObject other_object = object;
    sm64_objects_set_animation_tick(other, 50);
    sm64_objects_activate(other);
    sm64_physics_advance_object_animation(&other_object);
    CHECK(other_object.animInfo.animTimer == 50 && other_object.animInfo.animFrame == 4);
    sm64_objects_activate(world);
    sm64_physics_advance_object_animation(&object);
    CHECK(object.animInfo.animTimer == 0 && object.animInfo.animFrameAccelAssist == 0x38000);
    CHECK(sm64_objects_animation_tick(other) == 50);
    sm64_objects_destroy(other);

    u16 attributes[] = {3, 10, 1, 20};
    u16 *cursor = attributes;
    CHECK(retrieve_animation_index(2, &cursor) == 12 && cursor == attributes + 2);
    CHECK(retrieve_animation_index(30, &cursor) == 20 && cursor == attributes + 4);
    CHECK(segmented_to_virtual(&animation) == &animation);
    CHECK(virtual_to_segmented(7, &animation) == &animation);
    puts("Native animation: forward, reverse, clamp, loop, fractional speed and tick ownership passed");
    sm64_objects_activate(NULL);
    sm64_objects_destroy(world);
    return 0;
}
