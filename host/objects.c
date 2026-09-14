#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "objects_state.h"

_Thread_local struct sm64_objects *sm64_active_objects;
struct sm64_objects *sm64_objects_activate(struct sm64_objects *objects) {
    struct sm64_objects *previous = sm64_active_objects;
    sm64_active_objects = objects;
    if (objects) {
        gMarioObject = objects->mario;
        gCurrentObject = objects->current;
    }
    return previous;
}

static struct GraphNode *sDefaultEmptyGraphNodes[256];

struct sm64_objects *sm64_objects_create(void) {
    struct sm64_objects *objects = calloc(1, sizeof(*objects));
    if (!objects) return NULL;
    objects->swim_strength = 160;
    objects->submerged_init = 1;
    objects->models = sDefaultEmptyGraphNodes;
    struct sm64_objects *previous = sm64_objects_activate(objects);
    objects->level = LEVEL_BOB;
    objects->active_lists = objects->lists;
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

void sm64_objects_set_frame_info(struct sm64_objects *objects, u32 frame, u8 debug_level_select) {
    assert(objects);
    objects->global_timer = frame;
    objects->debug_level_select = debug_level_select;
}

void sm64_objects_set_mario(struct sm64_objects *objects, struct Object *mario) {
    assert(objects);
    objects->mario = mario;
    if (sm64_active_objects == objects) {
        gMarioObject = mario;
    }
}

void sm64_objects_set_motion_state(struct sm64_objects *objects, struct MarioState *mario,
                                   struct Object *current, u32 time_stop) {
    assert(objects);
    objects->mario_state = mario;
    objects->current = current;
    if (sm64_active_objects == objects) {
        gCurrentObject = current;
    }
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

static inline void *rebase_obj(const struct sm64_objects *src, struct sm64_objects *dst, void *ptr) {
    if (!ptr) return NULL;
    uintptr_t u = (uintptr_t)ptr;
    uintptr_t start = (uintptr_t)src;
    if (u >= start && u < start + sizeof(struct sm64_objects)) {
        return (void *)((uintptr_t)dst + (u - start));
    }
    return ptr;
}

void sm64_objects_copy(struct sm64_objects *dst, const struct sm64_objects *src) {
    if (!dst || !src || dst == src) return;
    memcpy(dst, src, sizeof(*dst));

    dst->active_lists = (struct ObjectNode *)rebase_obj(src, dst, src->active_lists);
    dst->current = (struct Object *)rebase_obj(src, dst, src->current);
    dst->mario = (struct Object *)rebase_obj(src, dst, src->mario);
    dst->mario_platform = (struct Object *)rebase_obj(src, dst, src->mario_platform);

    dst->parent.prev = (struct GraphNode *)rebase_obj(src, dst, src->parent.prev);
    dst->parent.next = (struct GraphNode *)rebase_obj(src, dst, src->parent.next);
    dst->parent.parent = (struct GraphNode *)rebase_obj(src, dst, src->parent.parent);
    dst->parent.children = (struct GraphNode *)rebase_obj(src, dst, src->parent.children);

    dst->free_list.next = (struct ObjectNode *)rebase_obj(src, dst, src->free_list.next);
    dst->free_list.prev = (struct ObjectNode *)rebase_obj(src, dst, src->free_list.prev);

    for (unsigned l = 0; l < NUM_OBJ_LISTS; ++l) {
        dst->lists[l].next = (struct ObjectNode *)rebase_obj(src, dst, src->lists[l].next);
        dst->lists[l].prev = (struct ObjectNode *)rebase_obj(src, dst, src->lists[l].prev);
    }

    for (size_t i = 0; i < OBJECT_POOL_CAPACITY; ++i) {
        struct Object *o = &dst->pool[i];
        o->header.next = (struct ObjectNode *)rebase_obj(src, dst, o->header.next);
        o->header.prev = (struct ObjectNode *)rebase_obj(src, dst, o->header.prev);

        o->header.gfx.node.prev = (struct GraphNode *)rebase_obj(src, dst, o->header.gfx.node.prev);
        o->header.gfx.node.next = (struct GraphNode *)rebase_obj(src, dst, o->header.gfx.node.next);
        o->header.gfx.node.parent = (struct GraphNode *)rebase_obj(src, dst, o->header.gfx.node.parent);
        o->header.gfx.node.children = (struct GraphNode *)rebase_obj(src, dst, o->header.gfx.node.children);

        o->header.gfx.throwMatrix = (Mat4 *)rebase_obj(src, dst, o->header.gfx.throwMatrix);

        o->parentObj = (struct Object *)rebase_obj(src, dst, o->parentObj);
        o->prevObj = (struct Object *)rebase_obj(src, dst, o->prevObj);
        o->platform = (struct Object *)rebase_obj(src, dst, o->platform);

        for (int c = 0; c < 4; ++c) {
            o->collidedObjs[c] = (struct Object *)rebase_obj(src, dst, o->collidedObjs[c]);
        }

#if IS_64_BIT
        for (int p = 0; p < 0x50; ++p) {
            o->ptrData.asVoidPtr[p] = rebase_obj(src, dst, o->ptrData.asVoidPtr[p]);
        }
#endif
    }
}

struct sm64_objects *sm64_objects_clone(const struct sm64_objects *src) {
    if (!src) return NULL;
    struct sm64_objects *dst = calloc(1, sizeof(*dst));
    if (!dst) return NULL;
    sm64_objects_copy(dst, src);
    return dst;
}

