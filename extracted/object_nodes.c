/* Generated verbatim upstream function extraction. See extracted.json. */
#include "sm64.h"
#include "engine/graph_node.h"
#include "engine/math_util.h"
#include "engine/geo_layout.h"
#include "game/area.h"
#include "game/memory.h"

#line 25 "n64decomp/src/engine/graph_node.c"
void init_scene_graph_node_links(struct GraphNode *graphNode, s32 type) {
    graphNode->type = type;
    graphNode->flags = GRAPH_RENDER_ACTIVE;
    graphNode->prev = graphNode;
    graphNode->next = graphNode;
    graphNode->parent = NULL;
    graphNode->children = NULL;
}

#line 302 "n64decomp/src/engine/graph_node.c"
struct GraphNodeObject *init_graph_node_object(struct AllocOnlyPool *pool,
                                               struct GraphNodeObject *graphNode,
                                               struct GraphNode *sharedChild, Vec3f pos, Vec3s angle,
                                               Vec3f scale) {
    if (pool != NULL) {
        graphNode = alloc_only_pool_alloc(pool, sizeof(struct GraphNodeObject));
    }

    if (graphNode != NULL) {
        init_scene_graph_node_links(&graphNode->node, GRAPH_NODE_TYPE_OBJECT);
        vec3f_copy(graphNode->pos, pos);
        vec3f_copy(graphNode->scale, scale);
        vec3s_copy(graphNode->angle, angle);
        graphNode->sharedChild = sharedChild;
        graphNode->throwMatrix = NULL;
        graphNode->animInfo.animID = 0;
        graphNode->animInfo.curAnim = NULL;
        graphNode->animInfo.animFrame = 0;
        graphNode->animInfo.animFrameAccelAssist = 0;
        graphNode->animInfo.animAccel = 0x10000;
        graphNode->animInfo.animTimer = 0;
        graphNode->node.flags |= GRAPH_RENDER_HAS_ANIMATION;
    }

    return graphNode;
}

#line 525 "n64decomp/src/engine/graph_node.c"
struct GraphNode *geo_add_child(struct GraphNode *parent, struct GraphNode *childNode) {
    struct GraphNode *parentFirstChild;
    struct GraphNode *parentLastChild;

    if (childNode != NULL) {
        childNode->parent = parent;
        parentFirstChild = parent->children;

        if (parentFirstChild == NULL) {
            parent->children = childNode;
            childNode->prev = childNode;
            childNode->next = childNode;
        } else {
            parentLastChild = parentFirstChild->prev;
            childNode->prev = parentLastChild;
            childNode->next = parentFirstChild;
            parentFirstChild->prev = childNode;
            parentLastChild->next = childNode;
        }
    }

    return childNode;
}

#line 555 "n64decomp/src/engine/graph_node.c"
struct GraphNode *geo_remove_child(struct GraphNode *graphNode) {
    struct GraphNode *parent;
    struct GraphNode **firstChild;

    parent = graphNode->parent;
    firstChild = &parent->children;

    // Remove link with siblings
    graphNode->prev->next = graphNode->next;
    graphNode->next->prev = graphNode->prev;

    // If this node was the first child, a new first child must be chosen
    if (*firstChild == graphNode) {
        // The list is circular, so this checks whether it was the only child
        if (graphNode->next == graphNode) {
            *firstChild = NULL; // Parent has no children anymore
        } else {
            *firstChild = graphNode->next; // Choose a new first child
        }
    }

    return parent;
}

#line 585 "n64decomp/src/engine/graph_node.c"
struct GraphNode *geo_make_first_child(struct GraphNode *newFirstChild) {
    struct GraphNode *lastSibling;
    struct GraphNode *parent;
    struct GraphNode **firstChild;

    parent = newFirstChild->parent;
    firstChild = &parent->children;

    if (*firstChild != newFirstChild) {
        if ((*firstChild)->prev != newFirstChild) {
            newFirstChild->prev->next = newFirstChild->next;
            newFirstChild->next->prev = newFirstChild->prev;
            lastSibling = (*firstChild)->prev;
            newFirstChild->prev = lastSibling;
            newFirstChild->next = *firstChild;
            (*firstChild)->prev = newFirstChild;
            lastSibling->next = newFirstChild;
        }
        *firstChild = newFirstChild;
    }

    return parent;
}

#line 683 "n64decomp/src/engine/graph_node.c"
void geo_reset_object_node(struct GraphNodeObject *graphNode) {
    init_graph_node_object(NULL, graphNode, 0, gVec3fZero, gVec3sZero, gVec3fOne);

    geo_add_child(&gObjParentGraphNode, &graphNode->node);
    graphNode->node.flags &= ~GRAPH_RENDER_ACTIVE;
}

#line 693 "n64decomp/src/engine/graph_node.c"
void geo_obj_init(struct GraphNodeObject *graphNode, void *sharedChild, Vec3f pos, Vec3s angle) {
    vec3f_set(graphNode->scale, 1.0f, 1.0f, 1.0f);
    vec3f_copy(graphNode->pos, pos);
    vec3s_copy(graphNode->angle, angle);

    graphNode->sharedChild = sharedChild;
    graphNode->unk4C = 0;
    graphNode->throwMatrix = NULL;
    graphNode->animInfo.curAnim = NULL;

    graphNode->node.flags |= GRAPH_RENDER_ACTIVE;
    graphNode->node.flags &= ~GRAPH_RENDER_INVISIBLE;
    graphNode->node.flags |= GRAPH_RENDER_HAS_ANIMATION;
    graphNode->node.flags &= ~GRAPH_RENDER_BILLBOARD;
}

#line 712 "n64decomp/src/engine/graph_node.c"
void geo_obj_init_spawninfo(struct GraphNodeObject *graphNode, struct SpawnInfo *spawn) {
    vec3f_set(graphNode->scale, 1.0f, 1.0f, 1.0f);
    vec3s_copy(graphNode->angle, spawn->startAngle);

    graphNode->pos[0] = (f32) spawn->startPos[0];
    graphNode->pos[1] = (f32) spawn->startPos[1];
    graphNode->pos[2] = (f32) spawn->startPos[2];

    graphNode->areaIndex = spawn->areaIndex;
    graphNode->activeAreaIndex = spawn->activeAreaIndex;
    graphNode->sharedChild = spawn->model;
    graphNode->unk4C = spawn;
    graphNode->throwMatrix = NULL;
    graphNode->animInfo.curAnim = 0;

    graphNode->node.flags |= GRAPH_RENDER_ACTIVE;
    graphNode->node.flags &= ~GRAPH_RENDER_INVISIBLE;
    graphNode->node.flags |= GRAPH_RENDER_HAS_ANIMATION;
    graphNode->node.flags &= ~GRAPH_RENDER_BILLBOARD;
}
