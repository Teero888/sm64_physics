#include <assert.h>
#include <stdio.h>
#include "../host/objects.h"
#include "../host/audio.h"
#include "../host/terrain.h"
#include "engine/graph_node.h"
#include "game/object_list_processor.h"
#include "level_table.h"

static const BehaviorScript regular[] = {OBJ_LIST_DEFAULT << 16};
static const BehaviorScript disposable[] = {OBJ_LIST_UNIMPORTANT << 16};
static const BehaviorScript actor[] = {OBJ_LIST_GENACTOR << 16};
static size_t list_count(struct ObjectNode *head) {
    size_t count = 0;
    for (struct ObjectNode *node = head->next; node != head; node = node->next) {
        assert(node->next->prev == node && node->prev->next == node);
        assert(++count <= OBJECT_POOL_CAPACITY);
    }
    return count;
}

int main(void) {
    struct sm64_audio_state *audio = sm64_audio_create(NULL, NULL);
    struct sm64_objects *a = sm64_objects_create(), *b = sm64_objects_create();
    assert(audio && a && b);
    sm64_audio_activate(audio);
    assert(sm64_objects_activate(a) == NULL);
    struct Object *first = sm64_objects_spawn(regular);
    assert(first && first->behavior == regular && first->parentObj == first);
    assert(first->oHealth == 2048 && first->oIntangibleTimer == -1);
    assert(first->hitboxRadius == 50 && first->oDrawingDistance == 4000);
    first->oHealth = 1;
    first->collisionData = (void *)regular;
    mark_obj_for_deletion(first);
    assert(first->activeFlags == ACTIVE_FLAG_DEACTIVATED);
    assert(list_count(sm64_objects_list(a, OBJ_LIST_DEFAULT)) == 1);
    unload_object(first);
    assert(list_count(sm64_objects_list(a, OBJ_LIST_DEFAULT)) == 0);
    sm64_objects_set_level(a, LEVEL_TTC);
    assert(sm64_objects_spawn(regular) == first);
    assert(first->oHealth == 2048 && first->collisionData == NULL);
    assert(first->oDrawingDistance == 2000);
    struct Object *evicted = sm64_objects_spawn(disposable);
    assert(evicted && (evicted->activeFlags & ACTIVE_FLAG_UNIMPORTANT));
    for (int i = 2; i < OBJECT_POOL_CAPACITY; ++i) assert(sm64_objects_spawn(regular));
    assert(list_count(sm64_objects_list(a, OBJ_LIST_DEFAULT)) == OBJECT_POOL_CAPACITY - 1);
    assert(sm64_objects_spawn(regular) == evicted);
    assert(!(evicted->activeFlags & ACTIVE_FLAG_UNIMPORTANT));
    assert(list_count(sm64_objects_list(a, OBJ_LIST_UNIMPORTANT)) == 0);
    assert(list_count(sm64_objects_list(a, OBJ_LIST_DEFAULT)) == OBJECT_POOL_CAPACITY);
    assert(sm64_objects_spawn(regular) == NULL);
    assert(sm64_objects_spawn(NULL) == NULL);
    const BehaviorScript invalid[] = {NUM_OBJ_LISTS << 16};
    assert(sm64_objects_spawn(invalid) == NULL);
    unload_object(first);
    assert(sm64_objects_spawn(regular) == first);

    assert(sm64_objects_activate(b) == a);
    struct sm64_terrain_triangle triangle = {
        .vertices = {{-100, 1, -100}, {0, 1, 100}, {100, 1, -100}}, .type = SURFACE_DEFAULT};
    struct sm64_terrain *terrain = sm64_terrain_create(&triangle, 1, NULL, 0);
    assert(terrain);
    sm64_terrain_activate(terrain);
    struct Object *second = sm64_objects_spawn(actor);
    assert(second && second != first);
    assert(second->oPosY == 1 && (second->oMoveFlags & OBJ_MOVE_ON_GROUND));
    assert(second->header.gfx.node.parent != first->header.gfx.node.parent);
    assert(list_count(sm64_objects_list(b, OBJ_LIST_GENACTOR)) == 1);
    sm64_objects_destroy(a);
    unload_object(second);
    assert(list_count(sm64_objects_list(b, OBJ_LIST_GENACTOR)) == 0);
    assert(sm64_objects_spawn(regular) == second);
    sm64_objects_destroy(b);
    sm64_terrain_destroy(terrain);
    sm64_audio_destroy(audio);
    puts("Native object lifetime: initialization, reuse, eviction, exhaustion, floor snap and independent owners passed");
    return 0;
}
