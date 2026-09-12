#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include "sm64.h"
#include "engine/graph_node.h"
#include "engine/geo_layout.h"
#include "engine/math_util.h"
#include "game/area.h"
#include "game/memory.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

int main(void) {
    struct GraphNodeObject a = {0}, b = {0}, c = {0};
    init_scene_graph_node_links(&gObjParentGraphNode, GRAPH_NODE_TYPE_OBJECT_PARENT);
    geo_reset_object_node(&a);
    geo_reset_object_node(&b);
    geo_reset_object_node(&c);
    CHECK(gObjParentGraphNode.children == &a.node);
    CHECK(a.node.next == &b.node && b.node.next == &c.node && c.node.next == &a.node);
    CHECK(a.node.prev == &c.node && c.node.prev == &b.node && b.node.prev == &a.node);
    CHECK(!(a.node.flags & GRAPH_RENDER_ACTIVE));
    CHECK(a.scale[0] == 1 && a.scale[1] == 1 && a.scale[2] == 1);
    geo_make_first_child(&b.node);
    CHECK(gObjParentGraphNode.children == &b.node);
    CHECK(b.node.next == &a.node && a.node.next == &c.node && c.node.next == &b.node);
    geo_remove_child(&a.node);
    CHECK(b.node.next == &c.node && c.node.next == &b.node);
    geo_remove_child(&b.node);
    CHECK(gObjParentGraphNode.children == &c.node && c.node.next == &c.node);
    geo_remove_child(&c.node);
    CHECK(gObjParentGraphNode.children == NULL);

    struct SpawnInfo spawn = {.startPos = {10, 20, -30}, .startAngle = {100, 200, 300},
                              .areaIndex = 2, .activeAreaIndex = 3};
    geo_obj_init_spawninfo(&a, &spawn);
    CHECK(a.pos[0] == 10 && a.pos[1] == 20 && a.pos[2] == -30);
    CHECK(a.angle[0] == 100 && a.angle[1] == 200 && a.angle[2] == 300);
    CHECK(a.areaIndex == 2 && a.activeAreaIndex == 3 && a.unk4C == &spawn);
    CHECK(a.node.flags & GRAPH_RENDER_HAS_ANIMATION);

    /* Native allocation must stay aligned after arbitrary byte-sized data. */
    _Alignas(max_align_t) unsigned char storage[1024];
    struct AllocOnlyPool pool = {.totalSpace = sizeof(storage),
                                .startPtr = storage, .freePtr = storage};
    CHECK(alloc_only_pool_alloc(&pool, 3) == storage);
    struct GraphNodeObject *allocated = init_graph_node_object(
        &pool, NULL, NULL, gVec3fZero, gVec3sZero, gVec3fOne);
    CHECK(allocated != NULL && (uintptr_t) allocated % _Alignof(max_align_t) == 0);
    CHECK(allocated->node.next == &allocated->node && allocated->animInfo.curAnim == NULL);
    const int used = pool.usedSpace;
    void *free_before = pool.freePtr;
    CHECK(alloc_only_pool_alloc(&pool, 1024) == NULL);
    CHECK(alloc_only_pool_alloc(&pool, -1) == NULL);
    CHECK(pool.usedSpace == used && pool.freePtr == free_before);

    Mat4 matrix;
    Vec3f translation = {10, 20, 30};
    Vec3s point = {1, 2, 3};
    Vec3s angle = {0, 0, 0};
    mtxf_rotate_zxy_and_translate(matrix, translation, angle);
    mtxf_mul_vec3s(matrix, angle);
    CHECK(angle[0] == 10 && angle[1] == 20 && angle[2] == 30);
    mtxf_mul_vec3s(matrix, point);
    CHECK(point[0] == 11 && point[1] == 22 && point[2] == 33);
    puts("Native object nodes: links, spawn transforms, aligned allocation and matrix math passed");
    return 0;
}
