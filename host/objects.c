#include <assert.h>
#include <stdlib.h>
#include "objects_state.h"

_Thread_local struct sm64_objects *sm64_active_objects;
struct sm64_objects *sm64_objects_activate(struct sm64_objects *objects) {
    struct sm64_objects *previous = sm64_active_objects;
    sm64_active_objects = objects;
    return previous;
}

struct sm64_objects *sm64_objects_create(void) {
    struct sm64_objects *objects = calloc(1, sizeof(*objects));
    if (!objects) return NULL;
    struct sm64_objects *previous = sm64_objects_activate(objects);
    objects->level = LEVEL_BOB;
    init_free_object_list();
    clear_object_lists(objects->lists);
    /* The original allocator requires every free slot to already have a
     * linked bookkeeping node. These nodes hold no GPU resources. */
    Vec3f zero = {0}, one = {1, 1, 1};
    Vec3s angles = {0};
    for (size_t i = 0; i < OBJECT_POOL_CAPACITY; ++i) {
        struct GraphNodeObject *node = &objects->pool[i].header.gfx;
        init_graph_node_object(NULL, node, NULL, zero, angles, one);
        geo_add_child(&objects->parent, &node->node);
        node->node.flags &= ~GRAPH_RENDER_ACTIVE;
    }
    sm64_objects_activate(previous);
    return objects;
}

void sm64_objects_destroy(struct sm64_objects *objects) {
    if (sm64_active_objects == objects) sm64_active_objects = NULL;
    free(objects);
}

void sm64_objects_set_level(struct sm64_objects *objects, s16 level) {
    assert(objects);
    objects->level = level;
}

void sm64_objects_set_random_seed(struct sm64_objects *objects, u16 seed) {
    assert(objects);
    objects->random_seed = seed;
}

u16 sm64_objects_random_seed(const struct sm64_objects *objects) {
    assert(objects);
    return objects->random_seed;
}

void sm64_objects_set_mario(struct sm64_objects *objects, struct Object *mario) {
    assert(objects);
    objects->mario = mario;
}

void sm64_objects_set_motion_state(struct sm64_objects *objects, struct MarioState *mario,
                                   struct Object *current, u32 time_stop) {
    assert(objects);
    objects->mario_state = mario;
    objects->current = current;
    objects->time_stop = time_stop;
}

struct ObjectNode *sm64_objects_list(struct sm64_objects *objects, unsigned list) {
    return objects && list < NUM_OBJ_LISTS ? &objects->lists[list] : NULL;
}

struct Object *sm64_objects_spawn(const BehaviorScript *behavior) {
    assert(sm64_active_objects);
    if (!behavior) return NULL;
    unsigned list = behavior[0] >> 24 == 0 ? (behavior[0] >> 16) & 0xffff : OBJ_LIST_DEFAULT;
    if (list >= NUM_OBJ_LISTS) return NULL;
    if (!sm64_active_objects->free_list.next && !find_unimportant_object()) return NULL;
    return create_object(behavior);
}
