#include <assert.h>
#include "behavior.h"
#include "behavior_state.h"
void sm64_objects_bind_behavior_assets(struct sm64_objects *objects, struct GraphNode **models,
    const BehaviorScript *chair, const BehaviorScript *piano, const BehaviorScript *panel) {
    assert(objects);
    objects->models = models;
    objects->haunted_chair = chair;
    objects->mad_piano = piano;
    objects->message_panel = panel;
}
void sm64_objects_update_behavior(struct Object *object, u32 frame) {
    assert(sm64_active_objects && object);
    sm64_active_objects->current = object;
    sm64_active_objects->global_timer = frame;
    cur_obj_update();
}
