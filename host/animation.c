#include "animation.h"
#include <assert.h>
#include "animation_state.h"

void sm64_objects_set_animation_tick(struct sm64_objects *objects, u16 tick) {
    assert(objects);
    objects->animation_tick = tick;
}
u16 sm64_objects_animation_tick(const struct sm64_objects *objects) {
    assert(objects);
    return objects->animation_tick;
}

void sm64_physics_advance_object_animation(struct GraphNodeObject *object) {
    assert(sm64_active_objects && object);
    if (object->animInfo.curAnim == NULL) {
        return;
    }
    /* These are the simulation mutations formerly performed by
     * geo_set_animation_globals in rendering_graph_node.c. The actual frame
     * calculation is the verbatim decompiled geo_update_animation_frame. */
    if (object->node.flags & GRAPH_RENDER_HAS_ANIMATION) {
        object->animInfo.animFrame = geo_update_animation_frame(
            &object->animInfo, &object->animInfo.animFrameAccelAssist);
    }
    object->animInfo.animTimer = gAreaUpdateCounter;
}
