#include "animation.h"
#include "game/area.h"

void sm64_physics_advance_object_animation(struct GraphNodeObject *object) {
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
