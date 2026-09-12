#include <stdio.h>
#include <stdlib.h>
#include "sm64.h"
#include "game/area.h"
#include "game/memory.h"
#include "../host/animation.h"

u16 gAreaUpdateCounter;

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

static void tick(struct GraphNodeObject *object) {
    ++gAreaUpdateCounter;
    sm64_physics_advance_object_animation(object);
}

int main(void) {
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
    CHECK(object.animInfo.animTimer == gAreaUpdateCounter);
    object.node.flags = GRAPH_RENDER_HAS_ANIMATION;
    gAreaUpdateCounter = 0xffff;
    object.animInfo.animTimer = 0xffff;
    tick(&object);
    CHECK(gAreaUpdateCounter == 0 && object.animInfo.animFrameAccelAssist == 0x38000);

    u16 attributes[] = {3, 10, 1, 20};
    u16 *cursor = attributes;
    CHECK(retrieve_animation_index(2, &cursor) == 12 && cursor == attributes + 2);
    CHECK(retrieve_animation_index(30, &cursor) == 20 && cursor == attributes + 4);
    CHECK(segmented_to_virtual(&animation) == &animation);
    CHECK(virtual_to_segmented(7, &animation) == &animation);
    puts("Native animation: forward, reverse, clamp, loop, fractional speed and tick ownership passed");
    return 0;
}
