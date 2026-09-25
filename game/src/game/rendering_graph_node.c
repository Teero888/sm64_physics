#include <PR/ultratypes.h>

#include "area.h"
#include "engine/math_util.h"
#include "game_init.h"
#include "gfx_dimensions.h"
#include "main.h"
#include "memory.h"
#include "print.h"
#include "rendering_graph_node.h"
#include "object_list_processor.h"
#include "behavior_actions.h"
#include "geo_misc.h"
#include "level_geo.h"
#include "mario_misc.h"
#include "moving_texture.h"
#include "screen_transition.h"
#include <stddef.h>
#include "shadow.h"
#include "sm64.h"

/**
 * This file contains the code that processes the scene graph for rendering.
 * The scene graph is responsible for drawing everything except the HUD / text boxes.
 * First the root of the scene graph is processed when geo_process_root
 * is called from level_script.c. The rest of the tree is traversed recursively
 * using the function geo_process_node_and_siblings, which switches over all
 * geo node types and calls a specialized function accordingly.
 * The types are defined in engine/graph_node.h
 *
 * The scene graph typically looks like:
 * - Root (viewport)
 *  - Master list
 *   - Ortho projection
 *    - Background (skybox)
 *  - Master list
 *   - Perspective
 *    - Camera
 *     - <area-specific display lists>
 *     - Object parent
 *      - <group with 240 object nodes>
 *  - Master list
 *   - Script node (Cannon overlay)
 *
 */

s16 gMatStackIndex;
Mat4 gMatStack[32];
Mtx *gMatStackFixed[32];

/**
 * Animation nodes have state in global variables, so this struct captures
 * the animation state so a 'context switch' can be made when rendering the
 * held object.
 */
struct GeoAnimState {
    /*0x00*/ u8 type;
    /*0x01*/ u8 enabled;
    /*0x02*/ s16 frame;
    /*0x04*/ f32 translationMultiplier;
    /*0x08*/ u16 *attribute;
    /*0x0C*/ s16 *data;
};

// For some reason, this is a GeoAnimState struct, but the current state consists
// of separate global variables. It won't match EU otherwise.
struct GeoAnimState gGeoTempState;

u8 gCurrAnimType;
u8 gCurrAnimEnabled;
s16 gCurrAnimFrame;
f32 gCurrAnimTranslationMultiplier;
u16 *gCurrAnimAttribute;
s16 *gCurrAnimData;

struct AllocOnlyPool *gDisplayListHeap;

struct RenderModeContainer {
    u32 modes[8];
};

/* Rendermode settings for cycle 1 for all 8 layers. */
struct RenderModeContainer renderModeTable_1Cycle[2] = { { {
    G_RM_OPA_SURF,
    G_RM_AA_OPA_SURF,
    G_RM_AA_OPA_SURF,
    G_RM_AA_OPA_SURF,
    G_RM_AA_TEX_EDGE,
    G_RM_AA_XLU_SURF,
    G_RM_AA_XLU_SURF,
    G_RM_AA_XLU_SURF,
    } },
    { {
    /* z-buffered */
    G_RM_ZB_OPA_SURF,
    G_RM_AA_ZB_OPA_SURF,
    G_RM_AA_ZB_OPA_DECAL,
    G_RM_AA_ZB_OPA_INTER,
    G_RM_AA_ZB_TEX_EDGE,
    G_RM_AA_ZB_XLU_SURF,
    G_RM_AA_ZB_XLU_DECAL,
    G_RM_AA_ZB_XLU_INTER,
    } } };

/* Rendermode settings for cycle 2 for all 8 layers. */
struct RenderModeContainer renderModeTable_2Cycle[2] = { { {
    G_RM_OPA_SURF2,
    G_RM_AA_OPA_SURF2,
    G_RM_AA_OPA_SURF2,
    G_RM_AA_OPA_SURF2,
    G_RM_AA_TEX_EDGE2,
    G_RM_AA_XLU_SURF2,
    G_RM_AA_XLU_SURF2,
    G_RM_AA_XLU_SURF2,
    } },
    { {
    /* z-buffered */
    G_RM_ZB_OPA_SURF2,
    G_RM_AA_ZB_OPA_SURF2,
    G_RM_AA_ZB_OPA_DECAL2,
    G_RM_AA_ZB_OPA_INTER2,
    G_RM_AA_ZB_TEX_EDGE2,
    G_RM_AA_ZB_XLU_SURF2,
    G_RM_AA_ZB_XLU_DECAL2,
    G_RM_AA_ZB_XLU_INTER2,
    } } };

struct GraphNodeRoot *gCurGraphNodeRoot = NULL;
struct GraphNodeMasterList *gCurGraphNodeMasterList = NULL;
struct GraphNodePerspective *gCurGraphNodeCamFrustum = NULL;
struct GraphNodeCamera *gCurGraphNodeCamera = NULL;
struct GraphNodeObject *gCurGraphNodeObject = NULL;
struct GraphNodeHeldObject *gCurGraphNodeHeldObject = NULL;
u16 gAreaUpdateCounter = 0;

#ifdef F3DEX_GBI_2
LookAt lookAt;
#endif

/**
 * Library: store the fixed point copy of the current matrix, which is only
 * drawn with (docs/changes.md 10).
 */
static void geo_set_fixed_matrix(void) {
    if (SM64_DRAW) {
        Mtx *fixed = alloc_display_list(sizeof(*fixed));

        mtxf_to_mtx(fixed, WORLD(gMatStack)[WORLD(gMatStackIndex)]);
        WORLD(gMatStackFixed)[WORLD(gMatStackIndex)] = fixed;
    }
}

/**
 * Process a master list node.
 */
static void geo_process_master_list_sub(struct GraphNodeMasterList *node) {
    struct DisplayListNode *currList;
    s32 i;
    s32 enableZBuffer = (node->node.flags & GRAPH_RENDER_Z_BUFFER) != 0;

    if (!SM64_DRAW) {
        return;
    }
    struct RenderModeContainer *modeList = &WORLD(renderModeTable_1Cycle)[enableZBuffer];
    struct RenderModeContainer *mode2List = &WORLD(renderModeTable_2Cycle)[enableZBuffer];

    // @bug This is where the LookAt values should be calculated but aren't.
    // As a result, environment mapping is broken on Fast3DEX2 without the
    // changes below.
#ifdef F3DEX_GBI_2
    Mtx lMtx;
    guLookAtReflect(&lMtx, &lookAt, 0, 0, 0, /* eye */ 0, 0, 1, /* at */ 1, 0, 0 /* up */);
#endif

    if (enableZBuffer != 0) {
        gDPPipeSync(WORLD(gDisplayListHead)++);
        gSPSetGeometryMode(WORLD(gDisplayListHead)++, G_ZBUFFER);
    }

    for (i = 0; i < GFX_NUM_MASTER_LISTS; i++) {
        if ((currList = node->listHeads[i]) != NULL) {
            gDPSetRenderMode(WORLD(gDisplayListHead)++, modeList->modes[i], mode2List->modes[i]);
            while (currList != NULL) {
                gSPMatrix(WORLD(gDisplayListHead)++, VIRTUAL_TO_PHYSICAL(currList->transform),
                          G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
                gSPDisplayList(WORLD(gDisplayListHead)++, currList->displayList);
                currList = currList->next;
            }
        }
    }
    if (enableZBuffer != 0) {
        gDPPipeSync(WORLD(gDisplayListHead)++);
        gSPClearGeometryMode(WORLD(gDisplayListHead)++, G_ZBUFFER);
    }
}

/**
 * Appends the display list to one of the master lists based on the layer
 * parameter. Look at the RenderModeContainer struct to see the corresponding
 * render modes of layers.
 */
static void geo_append_display_list(void *displayList, s16 layer) {
    if (!SM64_DRAW) {
        return;
    }

#ifdef F3DEX_GBI_2
    gSPLookAt(gDisplayListHead++, &lookAt);
#endif
    if (WORLD(gCurGraphNodeMasterList) != 0) {
        struct DisplayListNode *listNode =
            alloc_only_pool_alloc(WORLD(gDisplayListHeap), sizeof(struct DisplayListNode));

        listNode->transform = WORLD(gMatStackFixed)[WORLD(gMatStackIndex)];
        listNode->displayList = displayList;
        listNode->next = 0;
        if (WORLD(gCurGraphNodeMasterList)->listHeads[layer] == 0) {
            WORLD(gCurGraphNodeMasterList)->listHeads[layer] = listNode;
        } else {
            WORLD(gCurGraphNodeMasterList)->listTails[layer]->next = listNode;
        }
        WORLD(gCurGraphNodeMasterList)->listTails[layer] = listNode;
    }
}

/**
 * Process the master list node.
 */
static void geo_process_master_list(struct GraphNodeMasterList *node) {
    N64_STACK_FRAME(geo_process_master_list);
    s32 i;
    UNUSED u8 filler[4];

    if (WORLD(gCurGraphNodeMasterList) == NULL && node->node.children != NULL) {
        WORLD(gCurGraphNodeMasterList) = node;
        for (i = 0; i < GFX_NUM_MASTER_LISTS; i++) {
            node->listHeads[i] = NULL;
        }
        geo_process_node_and_siblings(node->node.children);
        geo_process_master_list_sub(node);
        WORLD(gCurGraphNodeMasterList) = NULL;
    }
}

/**
 * Process an orthographic projection node.
 */
static void geo_process_ortho_projection(struct GraphNodeOrthoProjection *node) {
    N64_STACK_FRAME(geo_process_ortho_projection);
    if (node->node.children != NULL) {
        if (SM64_DRAW) {
            Mtx *mtx = alloc_display_list(sizeof(*mtx));
            f32 left = (WORLD(gCurGraphNodeRoot)->x - WORLD(gCurGraphNodeRoot)->width) / 2.0f * node->scale;
            f32 right = (WORLD(gCurGraphNodeRoot)->x + WORLD(gCurGraphNodeRoot)->width) / 2.0f * node->scale;
            f32 top = (WORLD(gCurGraphNodeRoot)->y - WORLD(gCurGraphNodeRoot)->height) / 2.0f * node->scale;
            f32 bottom = (WORLD(gCurGraphNodeRoot)->y + WORLD(gCurGraphNodeRoot)->height) / 2.0f * node->scale;

            guOrtho(mtx, left, right, bottom, top, -2.0f, 2.0f, 1.0f);
            gSPPerspNormalize(WORLD(gDisplayListHead)++, 0xFFFF);
            gSPMatrix(WORLD(gDisplayListHead)++, VIRTUAL_TO_PHYSICAL(mtx), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        }

        geo_process_node_and_siblings(node->node.children);
    }
}

/**
 * Process a perspective projection node.
 */
static void geo_process_perspective(struct GraphNodePerspective *node) {
    N64_STACK_FRAME(geo_process_perspective);
    if (node->fnNode.func != NULL) {
        node->fnNode.func(GEO_CONTEXT_RENDER, &node->fnNode.node, WORLD(gMatStack)[WORLD(gMatStackIndex)]);
    }
    if (node->fnNode.node.children != NULL) {
        if (SM64_DRAW) {
            u16 perspNorm;
            Mtx *mtx = alloc_display_list(sizeof(*mtx));

#ifdef VERSION_EU
            f32 aspect = ((f32) gCurGraphNodeRoot->width / (f32) gCurGraphNodeRoot->height) * 1.1f;
#else
            f32 aspect = (f32) WORLD(gCurGraphNodeRoot)->width / (f32) WORLD(gCurGraphNodeRoot)->height;
#endif

            guPerspective(mtx, &perspNorm, node->fov, aspect, node->near, node->far, 1.0f);
            gSPPerspNormalize(WORLD(gDisplayListHead)++, perspNorm);

            gSPMatrix(WORLD(gDisplayListHead)++, VIRTUAL_TO_PHYSICAL(mtx), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        }

        WORLD(gCurGraphNodeCamFrustum) = node;
        geo_process_node_and_siblings(node->fnNode.node.children);
        WORLD(gCurGraphNodeCamFrustum) = NULL;
    }
}

/**
 * Process a level of detail node. From the current transformation matrix,
 * the perpendicular distance to the camera is extracted and the children
 * of this node are only processed if that distance is within the render
 * range of this node.
 */
static void geo_process_level_of_detail(struct GraphNodeLevelOfDetail *node) {
    N64_STACK_FRAME(geo_process_level_of_detail);
#ifdef GBI_FLOATS
    Mtx *mtx = gMatStackFixed[gMatStackIndex];
    s16 distanceFromCam = (s32) -mtx->m[3][2]; // z-component of the translation column
#else
    // The fixed point Mtx type is defined as 16 longs, but it's actually 16
    // shorts for the integer parts followed by 16 shorts for the fraction parts
    //! Library: the fixed point matrix only exists when drawing. Its integer
    //! part of z, from the float matrix as mtxf_to_mtx (guMtxF2L) converts it.
    //! Out of s32 range, the N64 crashes converting it (see mtxf_to_mtx).
    s32 fixedZ = (long) (WORLD(gMatStack)[WORLD(gMatStackIndex)][3][2] * (float) 0x00010000);
    s16 distanceFromCam = -GET_HIGH_S16_OF_32(fixedZ); // z-component of the translation column
#endif

    if (node->minDistance <= distanceFromCam && distanceFromCam < node->maxDistance) {
        if (node->node.children != 0) {
            geo_process_node_and_siblings(node->node.children);
        }
    }
}

/**
 * Process a switch case node. The node's selection function is called
 * if it is 0, and among the node's children, only the selected child is
 * processed next.
 */
static void geo_process_switch(struct GraphNodeSwitchCase *node) {
    N64_STACK_FRAME(geo_process_switch);
    struct GraphNode *selectedChild = node->fnNode.node.children;
    s32 i;

    if (node->fnNode.func != NULL) {
        node->fnNode.func(GEO_CONTEXT_RENDER, &node->fnNode.node, WORLD(gMatStack)[WORLD(gMatStackIndex)]);
    }
    for (i = 0; selectedChild != NULL && node->selectedCase > i; i++) {
        selectedChild = selectedChild->next;
    }
    if (selectedChild != NULL) {
        geo_process_node_and_siblings(selectedChild);
    }
}

/**
 * Process a camera node.
 */
static void geo_process_camera(struct GraphNodeCamera *node) {
    N64_STACK_FRAME(geo_process_camera);
    Mat4 cameraTransform;

    if (node->fnNode.func != NULL) {
        node->fnNode.func(GEO_CONTEXT_RENDER, &node->fnNode.node, WORLD(gMatStack)[WORLD(gMatStackIndex)]);
    }
    if (SM64_DRAW) {
        Mtx *rollMtx = alloc_display_list(sizeof(*rollMtx));

        mtxf_rotate_xy(rollMtx, node->rollScreen);

        gSPMatrix(WORLD(gDisplayListHead)++, VIRTUAL_TO_PHYSICAL(rollMtx), G_MTX_PROJECTION | G_MTX_MUL | G_MTX_NOPUSH);
    }

    mtxf_lookat(cameraTransform, node->pos, node->focus, node->roll);
    mtxf_mul(WORLD(gMatStack)[WORLD(gMatStackIndex) + 1], cameraTransform, WORLD(gMatStack)[WORLD(gMatStackIndex)]);
    WORLD(gMatStackIndex)++;
    if (SM64_DRAW) {
        // Library: the camera, for a renderer drawing from another one
        // (SM64_CAMERA_TAG, sm64_physics.h).
        Mat4 *camera = alloc_display_list(sizeof(*camera));
        if (camera != NULL) {
            mtxf_copy(*camera, WORLD(gMatStack)[WORLD(gMatStackIndex)]);
            Gfx *tag = WORLD(gDisplayListHead)++;
            tag->words.w0 = (uintptr_t) (u8) G_NOOP << 24 | 0x63616d;
            tag->words.w1 = (uintptr_t) camera;
        }
    }
    geo_set_fixed_matrix();
    if (node->fnNode.node.children != 0) {
        WORLD(gCurGraphNodeCamera) = node;
        node->matrixPtr = &WORLD(gMatStack)[WORLD(gMatStackIndex)];
        geo_process_node_and_siblings(node->fnNode.node.children);
        WORLD(gCurGraphNodeCamera) = NULL;
    }
    WORLD(gMatStackIndex)--;
}

/**
 * Process a translation / rotation node. A transformation matrix based
 * on the node's translation and rotation is created and pushed on both
 * the float and fixed point matrix stacks.
 * For the rest it acts as a normal display list node.
 */
static void geo_process_translation_rotation(struct GraphNodeTranslationRotation *node) {
    N64_STACK_FRAME(geo_process_translation_rotation);
    Mat4 mtxf;
    Vec3f translation;

    vec3s_to_vec3f(translation, node->translation);
    mtxf_rotate_zxy_and_translate(mtxf, translation, node->rotation);
    mtxf_mul(WORLD(gMatStack)[WORLD(gMatStackIndex) + 1], mtxf, WORLD(gMatStack)[WORLD(gMatStackIndex)]);
    WORLD(gMatStackIndex)++;
    geo_set_fixed_matrix();
    if (node->displayList != NULL) {
        geo_append_display_list(node->displayList, node->node.flags >> 8);
    }
    if (node->node.children != NULL) {
        geo_process_node_and_siblings(node->node.children);
    }
    WORLD(gMatStackIndex)--;
}

/**
 * Process a translation node. A transformation matrix based on the node's
 * translation is created and pushed on both the float and fixed point matrix stacks.
 * For the rest it acts as a normal display list node.
 */
static void geo_process_translation(struct GraphNodeTranslation *node) {
    N64_STACK_FRAME(geo_process_translation);
    Mat4 mtxf;
    Vec3f translation;

    vec3s_to_vec3f(translation, node->translation);
    mtxf_rotate_zxy_and_translate(mtxf, translation, WORLD(gVec3sZero));
    mtxf_mul(WORLD(gMatStack)[WORLD(gMatStackIndex) + 1], mtxf, WORLD(gMatStack)[WORLD(gMatStackIndex)]);
    WORLD(gMatStackIndex)++;
    geo_set_fixed_matrix();
    if (node->displayList != NULL) {
        geo_append_display_list(node->displayList, node->node.flags >> 8);
    }
    if (node->node.children != NULL) {
        geo_process_node_and_siblings(node->node.children);
    }
    WORLD(gMatStackIndex)--;
}

/**
 * Process a rotation node. A transformation matrix based on the node's
 * rotation is created and pushed on both the float and fixed point matrix stacks.
 * For the rest it acts as a normal display list node.
 */
static void geo_process_rotation(struct GraphNodeRotation *node) {
    N64_STACK_FRAME(geo_process_rotation);
    Mat4 mtxf;

    mtxf_rotate_zxy_and_translate(mtxf, WORLD(gVec3fZero), node->rotation);
    mtxf_mul(WORLD(gMatStack)[WORLD(gMatStackIndex) + 1], mtxf, WORLD(gMatStack)[WORLD(gMatStackIndex)]);
    WORLD(gMatStackIndex)++;
    geo_set_fixed_matrix();
    if (node->displayList != NULL) {
        geo_append_display_list(node->displayList, node->node.flags >> 8);
    }
    if (node->node.children != NULL) {
        geo_process_node_and_siblings(node->node.children);
    }
    WORLD(gMatStackIndex)--;
}

/**
 * Process a scaling node. A transformation matrix based on the node's
 * scale is created and pushed on both the float and fixed point matrix stacks.
 * For the rest it acts as a normal display list node.
 */
static void geo_process_scale(struct GraphNodeScale *node) {
    N64_STACK_FRAME(geo_process_scale);
    UNUSED Mat4 transform;
    Vec3f scaleVec;

    vec3f_set(scaleVec, node->scale, node->scale, node->scale);
    mtxf_scale_vec3f(WORLD(gMatStack)[WORLD(gMatStackIndex) + 1], WORLD(gMatStack)[WORLD(gMatStackIndex)], scaleVec);
    WORLD(gMatStackIndex)++;
    geo_set_fixed_matrix();
    if (node->displayList != NULL) {
        geo_append_display_list(node->displayList, node->node.flags >> 8);
    }
    if (node->node.children != NULL) {
        geo_process_node_and_siblings(node->node.children);
    }
    WORLD(gMatStackIndex)--;
}

/**
 * Process a billboard node. A transformation matrix is created that makes its
 * children face the camera, and it is pushed on the floating point and fixed
 * point matrix stacks.
 * For the rest it acts as a normal display list node.
 */
static void geo_process_billboard(struct GraphNodeBillboard *node) {
    N64_STACK_FRAME(geo_process_billboard);
    Vec3f translation;

    WORLD(gMatStackIndex)++;
    vec3s_to_vec3f(translation, node->translation);
    mtxf_billboard(WORLD(gMatStack)[WORLD(gMatStackIndex)], WORLD(gMatStack)[WORLD(gMatStackIndex) - 1], translation,
                   WORLD(gCurGraphNodeCamera)->roll);
    if (WORLD(gCurGraphNodeHeldObject) != NULL) {
        mtxf_scale_vec3f(WORLD(gMatStack)[WORLD(gMatStackIndex)], WORLD(gMatStack)[WORLD(gMatStackIndex)],
                         WORLD(gCurGraphNodeHeldObject)->objNode->header.gfx.scale);
    } else if (WORLD(gCurGraphNodeObject) != NULL) {
        mtxf_scale_vec3f(WORLD(gMatStack)[WORLD(gMatStackIndex)], WORLD(gMatStack)[WORLD(gMatStackIndex)],
                         WORLD(gCurGraphNodeObject)->scale);
    }

    geo_set_fixed_matrix();

    if (node->displayList != NULL) {
        geo_append_display_list(node->displayList, node->node.flags >> 8);
    }

    if (node->node.children != NULL) {
        geo_process_node_and_siblings(node->node.children);
    }

    WORLD(gMatStackIndex)--;
}

/**
 * Process a display list node. It draws a display list without first pushing
 * a transformation on the stack, so all transformations are inherited from the
 * parent node. It processes its children if it has them.
 */
static void geo_process_display_list(struct GraphNodeDisplayList *node) {
    N64_STACK_FRAME(geo_process_display_list);
    if (node->displayList != NULL) {
        geo_append_display_list(node->displayList, node->node.flags >> 8);
    }
    if (node->node.children != NULL) {
        geo_process_node_and_siblings(node->node.children);
    }
}

/**
 * Process a generated list. Instead of storing a pointer to a display list,
 * the list is generated on the fly by a function.
 */
static void geo_process_generated_list(struct GraphNodeGenerated *node) {
    N64_STACK_FRAME(geo_process_generated_list);
    if (node->fnNode.func != NULL) {
        Gfx *list = node->fnNode.func(GEO_CONTEXT_RENDER, &node->fnNode.node,
                                     (struct AllocOnlyPool *) WORLD(gMatStack)[WORLD(gMatStackIndex)]);

        if (list != NULL) {
            geo_append_display_list((void *) VIRTUAL_TO_PHYSICAL(list), node->fnNode.node.flags >> 8);
        }
    }
    if (node->fnNode.node.children != NULL) {
        geo_process_node_and_siblings(node->fnNode.node.children);
    }
}

/**
 * Process a background node. Tries to retrieve a background display list from
 * the function of the node. If that function is null or returns null, a black
 * rectangle is drawn instead.
 */
static void geo_process_background(struct GraphNodeBackground *node) {
    N64_STACK_FRAME(geo_process_background);
    Gfx *list = NULL;

    if (node->fnNode.func != NULL) {
        list = node->fnNode.func(GEO_CONTEXT_RENDER, &node->fnNode.node,
                                 (struct AllocOnlyPool *) WORLD(gMatStack)[WORLD(gMatStackIndex)]);
    }
    if (list != NULL) {
        geo_append_display_list((void *) VIRTUAL_TO_PHYSICAL(list), node->fnNode.node.flags >> 8);
    } else if (WORLD(gCurGraphNodeMasterList) != NULL && SM64_DRAW) {
#ifndef F3DEX_GBI_2E
        Gfx *gfxStart = alloc_display_list(sizeof(Gfx) * 7);
#else
        Gfx *gfxStart = alloc_display_list(sizeof(Gfx) * 8);
#endif
        Gfx *gfx = gfxStart;

        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_FILL);
        gDPSetFillColor(gfx++, node->background);
        gDPFillRectangle(gfx++, GFX_DIMENSIONS_RECT_FROM_LEFT_EDGE(0), BORDER_HEIGHT,
        GFX_DIMENSIONS_RECT_FROM_RIGHT_EDGE(0) - 1, SCREEN_HEIGHT - BORDER_HEIGHT - 1);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gSPEndDisplayList(gfx++);

        geo_append_display_list((void *) VIRTUAL_TO_PHYSICAL(gfxStart), 0);
    }
    if (node->fnNode.node.children != NULL) {
        geo_process_node_and_siblings(node->fnNode.node.children);
    }
}

/**
 * Render an animated part. The current animation state is not part of the node
 * but set in global variables. If an animated part is skipped, everything afterwards desyncs.
 */
static void geo_process_animated_part(struct GraphNodeAnimatedPart *node) {
    N64_STACK_FRAME(geo_process_animated_part);
    Mat4 matrix;
    Vec3s rotation;
    Vec3f translation;

    vec3s_copy(rotation, WORLD(gVec3sZero));
    vec3f_set(translation, node->translation[0], node->translation[1], node->translation[2]);
    if (WORLD(gCurrAnimType) == ANIM_TYPE_TRANSLATION) {
        translation[0] += WORLD(gCurrAnimData)[retrieve_animation_index(WORLD(gCurrAnimFrame), &WORLD(gCurrAnimAttribute))]
                          * WORLD(gCurrAnimTranslationMultiplier);
        translation[1] += WORLD(gCurrAnimData)[retrieve_animation_index(WORLD(gCurrAnimFrame), &WORLD(gCurrAnimAttribute))]
                          * WORLD(gCurrAnimTranslationMultiplier);
        translation[2] += WORLD(gCurrAnimData)[retrieve_animation_index(WORLD(gCurrAnimFrame), &WORLD(gCurrAnimAttribute))]
                          * WORLD(gCurrAnimTranslationMultiplier);
        WORLD(gCurrAnimType) = ANIM_TYPE_ROTATION;
    } else {
        if (WORLD(gCurrAnimType) == ANIM_TYPE_LATERAL_TRANSLATION) {
            translation[0] +=
                WORLD(gCurrAnimData)[retrieve_animation_index(WORLD(gCurrAnimFrame), &WORLD(gCurrAnimAttribute))]
                * WORLD(gCurrAnimTranslationMultiplier);
            WORLD(gCurrAnimAttribute) += 2;
            translation[2] +=
                WORLD(gCurrAnimData)[retrieve_animation_index(WORLD(gCurrAnimFrame), &WORLD(gCurrAnimAttribute))]
                * WORLD(gCurrAnimTranslationMultiplier);
            WORLD(gCurrAnimType) = ANIM_TYPE_ROTATION;
        } else {
            if (WORLD(gCurrAnimType) == ANIM_TYPE_VERTICAL_TRANSLATION) {
                WORLD(gCurrAnimAttribute) += 2;
                translation[1] +=
                    WORLD(gCurrAnimData)[retrieve_animation_index(WORLD(gCurrAnimFrame), &WORLD(gCurrAnimAttribute))]
                    * WORLD(gCurrAnimTranslationMultiplier);
                WORLD(gCurrAnimAttribute) += 2;
                WORLD(gCurrAnimType) = ANIM_TYPE_ROTATION;
            } else if (WORLD(gCurrAnimType) == ANIM_TYPE_NO_TRANSLATION) {
                WORLD(gCurrAnimAttribute) += 6;
                WORLD(gCurrAnimType) = ANIM_TYPE_ROTATION;
            }
        }
    }

    if (WORLD(gCurrAnimType) == ANIM_TYPE_ROTATION) {
        rotation[0] = WORLD(gCurrAnimData)[retrieve_animation_index(WORLD(gCurrAnimFrame), &WORLD(gCurrAnimAttribute))];
        rotation[1] = WORLD(gCurrAnimData)[retrieve_animation_index(WORLD(gCurrAnimFrame), &WORLD(gCurrAnimAttribute))];
        rotation[2] = WORLD(gCurrAnimData)[retrieve_animation_index(WORLD(gCurrAnimFrame), &WORLD(gCurrAnimAttribute))];
    }
    mtxf_rotate_xyz_and_translate(matrix, translation, rotation);
    mtxf_mul(WORLD(gMatStack)[WORLD(gMatStackIndex) + 1], matrix, WORLD(gMatStack)[WORLD(gMatStackIndex)]);
    WORLD(gMatStackIndex)++;
    geo_set_fixed_matrix();
    if (node->displayList != NULL) {
        geo_append_display_list(node->displayList, node->node.flags >> 8);
    }
    if (node->node.children != NULL) {
        geo_process_node_and_siblings(node->node.children);
    }
    WORLD(gMatStackIndex)--;
}

/**
 * Initialize the animation-related global variables for the currently drawn
 * object's animation.
 */
void geo_set_animation_globals(struct AnimInfo *node, s32 hasAnimation) {
    struct Animation *anim = node->curAnim;

    if (hasAnimation) {
        node->animFrame = geo_update_animation_frame(node, &node->animFrameAccelAssist);
    }
    node->animTimer = WORLD(gAreaUpdateCounter);
    if (anim->flags & ANIM_FLAG_HOR_TRANS) {
        WORLD(gCurrAnimType) = ANIM_TYPE_VERTICAL_TRANSLATION;
    } else if (anim->flags & ANIM_FLAG_VERT_TRANS) {
        WORLD(gCurrAnimType) = ANIM_TYPE_LATERAL_TRANSLATION;
    } else if (anim->flags & ANIM_FLAG_6) {
        WORLD(gCurrAnimType) = ANIM_TYPE_NO_TRANSLATION;
    } else {
        WORLD(gCurrAnimType) = ANIM_TYPE_TRANSLATION;
    }

    WORLD(gCurrAnimFrame) = node->animFrame;
    WORLD(gCurrAnimEnabled) = (anim->flags & ANIM_FLAG_5) == 0;
    WORLD(gCurrAnimAttribute) = segmented_to_virtual((void *) anim->index);
    WORLD(gCurrAnimData) = segmented_to_virtual((void *) anim->values);

    if (anim->animYTransDivisor == 0) {
        WORLD(gCurrAnimTranslationMultiplier) = 1.0f;
    } else {
        WORLD(gCurrAnimTranslationMultiplier) = (f32) node->animYTrans / (f32) anim->animYTransDivisor;
    }
}

/**
 * Process a shadow node. Renders a shadow under an object offset by the
 * translation of the first animated component and rotated according to
 * the floor below it.
 */
static void geo_process_shadow(struct GraphNodeShadow *node) {
    N64_STACK_FRAME(geo_process_shadow);
    //! Library: without drawing, only what the shadow does to the game: its
    //! find_floor clears gFindFloorIncludeSurfaceIntangible, which
    //! geo_switch_area sets, so the game's next floor query skips
    //! intangible floors again.
    if (!SM64_DRAW) {
        if (WORLD(gCurGraphNodeCamera) != NULL && WORLD(gCurGraphNodeObject) != NULL) {
            WORLD(gFindFloorIncludeSurfaceIntangible) = FALSE;
        }
        if (node->node.children != NULL) {
            geo_process_node_and_siblings(node->node.children);
        }
        return;
    }
    Gfx *shadowList;
    Mat4 mtxf;
    Vec3f shadowPos;
    Vec3f animOffset;
    f32 objScale;
    f32 shadowScale;
    f32 sinAng;
    f32 cosAng;
    struct GraphNode *geo;
    Mtx *mtx;

    if (WORLD(gCurGraphNodeCamera) != NULL && WORLD(gCurGraphNodeObject) != NULL) {
        if (WORLD(gCurGraphNodeHeldObject) != NULL) {
            get_pos_from_transform_mtx(shadowPos, WORLD(gMatStack)[WORLD(gMatStackIndex)],
                                       *WORLD(gCurGraphNodeCamera)->matrixPtr);
            shadowScale = node->shadowScale;
        } else {
            vec3f_copy(shadowPos, WORLD(gCurGraphNodeObject)->pos);
            shadowScale = node->shadowScale * WORLD(gCurGraphNodeObject)->scale[0];
        }

        objScale = 1.0f;
        if (WORLD(gCurrAnimEnabled)) {
            if (WORLD(gCurrAnimType) == ANIM_TYPE_TRANSLATION
                || WORLD(gCurrAnimType) == ANIM_TYPE_LATERAL_TRANSLATION) {
                geo = node->node.children;
                if (geo != NULL && geo->type == GRAPH_NODE_TYPE_SCALE) {
                    objScale = ((struct GraphNodeScale *) geo)->scale;
                }
                animOffset[0] =
                    WORLD(gCurrAnimData)[retrieve_animation_index(WORLD(gCurrAnimFrame), &WORLD(gCurrAnimAttribute))]
                    * WORLD(gCurrAnimTranslationMultiplier) * objScale;
                animOffset[1] = 0.0f;
                WORLD(gCurrAnimAttribute) += 2;
                animOffset[2] =
                    WORLD(gCurrAnimData)[retrieve_animation_index(WORLD(gCurrAnimFrame), &WORLD(gCurrAnimAttribute))]
                    * WORLD(gCurrAnimTranslationMultiplier) * objScale;
                WORLD(gCurrAnimAttribute) -= 6;

                // simple matrix rotation so the shadow offset rotates along with the object
                sinAng = sins(WORLD(gCurGraphNodeObject)->angle[1]);
                cosAng = coss(WORLD(gCurGraphNodeObject)->angle[1]);

                shadowPos[0] += animOffset[0] * cosAng + animOffset[2] * sinAng;
                shadowPos[2] += -animOffset[0] * sinAng + animOffset[2] * cosAng;
            }
        }

        shadowList = create_shadow_below_xyz(shadowPos[0], shadowPos[1], shadowPos[2], shadowScale,
                                             node->shadowSolidity, node->shadowType);
        if (shadowList != NULL) {
            mtx = alloc_display_list(sizeof(*mtx));
            WORLD(gMatStackIndex)++;
            mtxf_translate(mtxf, shadowPos);
            mtxf_mul(WORLD(gMatStack)[WORLD(gMatStackIndex)], mtxf, *WORLD(gCurGraphNodeCamera)->matrixPtr);
            mtxf_to_mtx(mtx, WORLD(gMatStack)[WORLD(gMatStackIndex)]);
            WORLD(gMatStackFixed)[WORLD(gMatStackIndex)] = mtx;
            if (WORLD(gShadowAboveWaterOrLava) == TRUE) {
                geo_append_display_list((void *) VIRTUAL_TO_PHYSICAL(shadowList), 4);
            } else if (WORLD(gMarioOnIceOrCarpet) == 1) {
                geo_append_display_list((void *) VIRTUAL_TO_PHYSICAL(shadowList), 5);
            } else {
                geo_append_display_list((void *) VIRTUAL_TO_PHYSICAL(shadowList), 6);
            }
            WORLD(gMatStackIndex)--;
        }
    }

    if (node->node.children != NULL) {
        geo_process_node_and_siblings(node->node.children);
    }
}

/**
 * Check whether an object is in view to determine whether it should be drawn.
 * This is known as frustum culling.
 * It checks whether the object is far away, very close / behind the camera,
 * or horizontally out of view. It does not check whether it is vertically
 * out of view. It assumes a sphere of 300 units around the object's position
 * unless the object has a culling radius node that specifies otherwise.
 *
 * The matrix parameter should be the top of the matrix stack, which is the
 * object's transformation matrix times the camera 'look-at' matrix. The math
 * is counter-intuitive, but it checks column 3 (translation vector) of this
 * matrix to determine where the origin (0,0,0) in object space will be once
 * transformed to camera space (x+ = right, y+ = up, z = 'coming out the screen').
 * In 3D graphics, you typically model the world as being moved in front of a
 * static camera instead of a moving camera through a static world, which in
 * this case simplifies calculations. Note that the perspective matrix is not
 * on the matrix stack, so there are still calculations with the fov to compute
 * the slope of the lines of the frustum.
 *
 *        z-
 *
 *  \     |     /
 *   \    |    /
 *    \   |   /
 *     \  |  /
 *      \ | /
 *       \|/
 *        C       x+
 *
 * Since (0,0,0) is unaffected by rotation, columns 0, 1 and 2 are ignored.
 */
static s32 obj_is_in_view(struct GraphNodeObject *node, Mat4 matrix) {
    s16 cullingRadius;
    s16 halfFov; // half of the fov in in-game angle units instead of degrees
    struct GraphNode *geo;
    f32 hScreenEdge;

    if (node->node.flags & GRAPH_RENDER_INVISIBLE) {
        return FALSE;
    }

    geo = node->sharedChild;

    // ! @bug The aspect ratio is not accounted for. When the fov value is 45,
    // the horizontal effective fov is actually 60 degrees, so you can see objects
    // visibly pop in or out at the edge of the screen.
    halfFov = (WORLD(gCurGraphNodeCamFrustum)->fov / 2.0f + 1.0f) * 32768.0f / 180.0f + 0.5f;

    hScreenEdge = -matrix[3][2] * sins(halfFov) / coss(halfFov);
    // -matrix[3][2] is the depth, which gets multiplied by tan(halfFov) to get
    // the amount of units between the center of the screen and the horizontal edge
    // given the distance from the object to the camera.

#ifdef WIDESCREEN
    // This multiplication should really be performed on 4:3 as well,
    // but the issue will be more apparent on widescreen.
    hScreenEdge *= GFX_DIMENSIONS_ASPECT_RATIO;
#endif

    if (geo != NULL && geo->type == GRAPH_NODE_TYPE_CULLING_RADIUS) {
        cullingRadius =
            (f32)((struct GraphNodeCullingRadius *) geo)->cullingRadius; //! Why is there a f32 cast?
    } else {
        cullingRadius = 300;
    }

    // Don't render if the object is close to or behind the camera
    if (matrix[3][2] > -100.0f + cullingRadius) {
        return FALSE;
    }

    //! This makes the HOLP not update when the camera is far away, and it
    //  makes PU travel safe when the camera is locked on the main map.
    //  If Mario were rendered with a depth over 65536 it would cause overflow
    //  when converting the transformation matrix to a fixed point matrix.
    if (matrix[3][2] < -20000.0f - cullingRadius) {
        return FALSE;
    }

    // Check whether the object is horizontally in view
    if (matrix[3][0] > hScreenEdge + cullingRadius) {
        return FALSE;
    }
    if (matrix[3][0] < -hScreenEdge - cullingRadius) {
        return FALSE;
    }
    return TRUE;
}

/**
 * Library: what the render walk without drawing needs to know of a node and
 * its children, worked out on first use and kept in the node (docs/changes.md 12).
 * The shape of a model does not change once its geo layout is processed.
 */
#define HOST_WALK_KNOWN 0x01
// A node whose processing changes game state.
#define HOST_WALK_STATE 0x02
// A shadow: building one clears gFindFloorIncludeSurfaceIntangible.
#define HOST_WALK_SHADOW 0x04
// A shadow that every walk reaches (not below a switch or level of detail).
#define HOST_WALK_SHADOW_ALWAYS 0x08

_Static_assert(offsetof(struct GraphNode, prev) >= offsetof(struct GraphNode, hostWalk) + sizeof(u16),
               "hostWalk is in the padding");

/**
 * Library: geo functions that only draw (docs/changes.md 11).
 */
static s32 geo_func_only_draws(GraphNodeFunc func) {
    return func == NULL || func == (GraphNodeFunc) geo_skybox_main
           || func == (GraphNodeFunc) geo_mirror_mario_set_alpha
           || func == (GraphNodeFunc) geo_mirror_mario_backface_culling
           || func == (GraphNodeFunc) geo_cannon_circle_base
           || func == (GraphNodeFunc) geo_exec_inside_castle_light
           || func == (GraphNodeFunc) geo_exec_cake_end_screen
           || func == (GraphNodeFunc) geo_bits_bowser_coloring
           || func == (GraphNodeFunc) geo_movtex_draw_colored_no_update
           || func == (GraphNodeFunc) geo_movtex_draw_colored_2_no_update;
}

static u16 geo_walk_flags(struct GraphNode *node);

static u16 geo_walk_flags_of_siblings(struct GraphNode *first) {
    struct GraphNode *node = first;
    u16 flags = 0;

    if (first != NULL) {
        do {
            flags |= geo_walk_flags(node);
        } while ((node = node->next) != first);
    }
    return flags;
}

static u16 geo_walk_flags(struct GraphNode *node) {
    u16 own = 0;
    u16 children;

    if (node->hostWalk & HOST_WALK_KNOWN) {
        return node->hostWalk;
    }
    if (!(node->flags & GRAPH_RENDER_CHILDREN_FIRST)) {
        switch (node->type) {
            case GRAPH_NODE_TYPE_CAMERA:
            case GRAPH_NODE_TYPE_OBJECT:
            case GRAPH_NODE_TYPE_OBJECT_PARENT:
            case GRAPH_NODE_TYPE_HELD_OBJ:
                own = HOST_WALK_STATE;
                break;
            case GRAPH_NODE_TYPE_PERSPECTIVE:
            case GRAPH_NODE_TYPE_SWITCH_CASE:
            case GRAPH_NODE_TYPE_GENERATED_LIST:
            case GRAPH_NODE_TYPE_BACKGROUND:
                if (!geo_func_only_draws(((struct FnGraphNode *) node)->func)) {
                    own = HOST_WALK_STATE;
                }
                break;
            case GRAPH_NODE_TYPE_SHADOW:
                own = HOST_WALK_SHADOW | HOST_WALK_SHADOW_ALWAYS;
                break;
        }
    }
    children = geo_walk_flags_of_siblings(node->children);
    if (node->type == GRAPH_NODE_TYPE_SWITCH_CASE || node->type == GRAPH_NODE_TYPE_LEVEL_OF_DETAIL) {
        children &= ~HOST_WALK_SHADOW_ALWAYS;
    }
    node->hostWalk = own | children | HOST_WALK_KNOWN;
    // Not walked while inactive; only Mario's model changes that at run time.
    if (!(node->flags & GRAPH_RENDER_ACTIVE)) {
        node->hostWalk &= ~HOST_WALK_SHADOW_ALWAYS;
    }
    return node->hostWalk;
}

/**
 * Library: without drawing, an object whose model is not walked (out of view)
 * or changes no game state needs only what the walk leaves in it: its
 * position in camera space and its animation, and for a shadow in its model,
 * the flag that building the shadow clears. Returns FALSE if the object needs
 * the whole walk. A model is only looked at in view: out of view, it may be
 * one that is no longer loaded.
 */
static s32 geo_process_object_state(struct Object *node, s32 hasAnimation) {
    Mat4 *camera = &WORLD(gMatStack)[WORLD(gMatStackIndex)];
    Mat4 mtxf;
    f32 *position;
    s32 i;
    u16 flags = 0;

    if (node->header.gfx.node.children != NULL) {
        return FALSE;
    }
    // The translation row of the object's matrix as mtxf_mul (or
    // mtxf_billboard, the same operations) computes it; scaling keeps it.
    // obj_is_in_view only reads that row.
    position = node->header.gfx.throwMatrix != NULL ? (*node->header.gfx.throwMatrix)[3]
                                                    : node->header.gfx.pos;
    for (i = 0; i < 3; i++) {
        mtxf[3][i] = position[0] * (*camera)[0][i] + position[1] * (*camera)[1][i]
                     + position[2] * (*camera)[2][i] + (*camera)[3][i];
    }
    if (node->header.gfx.sharedChild != NULL && obj_is_in_view(&node->header.gfx, mtxf)) {
        flags = geo_walk_flags_of_siblings(node->header.gfx.sharedChild);
        if ((flags & HOST_WALK_STATE) || ((flags & HOST_WALK_SHADOW) && !(flags & HOST_WALK_SHADOW_ALWAYS))) {
            return FALSE;
        }
    }
    for (i = 0; i < 3; i++) {
        node->header.gfx.cameraToObject[i] = mtxf[3][i];
    }
    if (node->header.gfx.animInfo.curAnim != NULL) {
        geo_set_animation_globals(&node->header.gfx.animInfo, hasAnimation);
    }
    if ((flags & HOST_WALK_SHADOW_ALWAYS) && WORLD(gCurGraphNodeCamera) != NULL) {
        WORLD(gFindFloorIncludeSurfaceIntangible) = FALSE;
    }
    WORLD(gCurrAnimType) = ANIM_TYPE_NONE;
    node->header.gfx.throwMatrix = NULL;
    return TRUE;
}

/**
 * Process an object node.
 */
static void geo_process_object(struct Object *node) {
    N64_STACK_FRAME(geo_process_object);
    Mat4 mtxf;
    s32 hasAnimation = (node->header.gfx.node.flags & GRAPH_RENDER_HAS_ANIMATION) != 0;

    if (node->header.gfx.areaIndex == WORLD(gCurGraphNodeRoot)->areaIndex) {
        if (!SM64_DRAW && geo_process_object_state(node, hasAnimation)) {
            return;
        }
        if (node->header.gfx.throwMatrix != NULL) {
            mtxf_mul(WORLD(gMatStack)[WORLD(gMatStackIndex) + 1], *node->header.gfx.throwMatrix,
                     WORLD(gMatStack)[WORLD(gMatStackIndex)]);
        } else if (node->header.gfx.node.flags & GRAPH_RENDER_BILLBOARD) {
            mtxf_billboard(WORLD(gMatStack)[WORLD(gMatStackIndex) + 1], WORLD(gMatStack)[WORLD(gMatStackIndex)],
                           node->header.gfx.pos, WORLD(gCurGraphNodeCamera)->roll);
        } else {
            mtxf_rotate_zxy_and_translate(mtxf, node->header.gfx.pos, node->header.gfx.angle);
            mtxf_mul(WORLD(gMatStack)[WORLD(gMatStackIndex) + 1], mtxf, WORLD(gMatStack)[WORLD(gMatStackIndex)]);
        }

        mtxf_scale_vec3f(WORLD(gMatStack)[WORLD(gMatStackIndex) + 1], WORLD(gMatStack)[WORLD(gMatStackIndex) + 1],
                         node->header.gfx.scale);
        node->header.gfx.throwMatrix = &WORLD(gMatStack)[++WORLD(gMatStackIndex)];
        node->header.gfx.cameraToObject[0] = WORLD(gMatStack)[WORLD(gMatStackIndex)][3][0];
        node->header.gfx.cameraToObject[1] = WORLD(gMatStack)[WORLD(gMatStackIndex)][3][1];
        node->header.gfx.cameraToObject[2] = WORLD(gMatStack)[WORLD(gMatStackIndex)][3][2];

        // FIXME: correct types
        if (node->header.gfx.animInfo.curAnim != NULL) {
            geo_set_animation_globals(&node->header.gfx.animInfo, hasAnimation);
        }
        if (obj_is_in_view(&node->header.gfx, WORLD(gMatStack)[WORLD(gMatStackIndex)])) {
            geo_set_fixed_matrix();
            if (node->header.gfx.sharedChild != NULL) {
                WORLD(gCurGraphNodeObject) = (struct GraphNodeObject *) node;
                node->header.gfx.sharedChild->parent = &node->header.gfx.node;
                geo_process_node_and_siblings(node->header.gfx.sharedChild);
                node->header.gfx.sharedChild->parent = NULL;
                WORLD(gCurGraphNodeObject) = NULL;
            }
            if (node->header.gfx.node.children != NULL) {
                geo_process_node_and_siblings(node->header.gfx.node.children);
            }
        }

        WORLD(gMatStackIndex)--;
        WORLD(gCurrAnimType) = ANIM_TYPE_NONE;
        node->header.gfx.throwMatrix = NULL;
    }
}

/**
 * Process an object parent node. Temporarily assigns itself as the parent of
 * the subtree rooted at 'sharedChild' and processes the subtree, after which the
 * actual children are be processed. (in practice they are null though)
 */
static void geo_process_object_parent(struct GraphNodeObjectParent *node) {
    N64_STACK_FRAME(geo_process_object_parent);
    if (node->sharedChild != NULL) {
        node->sharedChild->parent = (struct GraphNode *) node;
        geo_process_node_and_siblings(node->sharedChild);
        node->sharedChild->parent = NULL;
    }
    if (node->node.children != NULL) {
        geo_process_node_and_siblings(node->node.children);
    }
}

/**
 * Process a held object node.
 */
void geo_process_held_object(struct GraphNodeHeldObject *node) {
    N64_STACK_FRAME(geo_process_held_object);
    Mat4 mat;
    Vec3f translation;

#ifdef F3DEX_GBI_2
    gSPLookAt(gDisplayListHead++, &lookAt);
#endif

    if (node->fnNode.func != NULL) {
        node->fnNode.func(GEO_CONTEXT_RENDER, &node->fnNode.node, WORLD(gMatStack)[WORLD(gMatStackIndex)]);
    }
    if (node->objNode != NULL && node->objNode->header.gfx.sharedChild != NULL) {
        s32 hasAnimation = (node->objNode->header.gfx.node.flags & GRAPH_RENDER_HAS_ANIMATION) != 0;

        translation[0] = node->translation[0] / 4.0f;
        translation[1] = node->translation[1] / 4.0f;
        translation[2] = node->translation[2] / 4.0f;

        mtxf_translate(mat, translation);
        mtxf_copy(WORLD(gMatStack)[WORLD(gMatStackIndex) + 1], *WORLD(gCurGraphNodeObject)->throwMatrix);
        WORLD(gMatStack)[WORLD(gMatStackIndex) + 1][3][0] = WORLD(gMatStack)[WORLD(gMatStackIndex)][3][0];
        WORLD(gMatStack)[WORLD(gMatStackIndex) + 1][3][1] = WORLD(gMatStack)[WORLD(gMatStackIndex)][3][1];
        WORLD(gMatStack)[WORLD(gMatStackIndex) + 1][3][2] = WORLD(gMatStack)[WORLD(gMatStackIndex)][3][2];
        mtxf_mul(WORLD(gMatStack)[WORLD(gMatStackIndex) + 1], mat, WORLD(gMatStack)[WORLD(gMatStackIndex) + 1]);
        mtxf_scale_vec3f(WORLD(gMatStack)[WORLD(gMatStackIndex) + 1], WORLD(gMatStack)[WORLD(gMatStackIndex) + 1],
                         node->objNode->header.gfx.scale);
        if (node->fnNode.func != NULL) {
            node->fnNode.func(GEO_CONTEXT_HELD_OBJ, &node->fnNode.node,
                              (struct AllocOnlyPool *) WORLD(gMatStack)[WORLD(gMatStackIndex) + 1]);
        }
        WORLD(gMatStackIndex)++;
        geo_set_fixed_matrix();
        WORLD(gGeoTempState).type = WORLD(gCurrAnimType);
        WORLD(gGeoTempState).enabled = WORLD(gCurrAnimEnabled);
        WORLD(gGeoTempState).frame = WORLD(gCurrAnimFrame);
        WORLD(gGeoTempState).translationMultiplier = WORLD(gCurrAnimTranslationMultiplier);
        WORLD(gGeoTempState).attribute = WORLD(gCurrAnimAttribute);
        WORLD(gGeoTempState).data = WORLD(gCurrAnimData);
        WORLD(gCurrAnimType) = 0;
        WORLD(gCurGraphNodeHeldObject) = (void *) node;
        if (node->objNode->header.gfx.animInfo.curAnim != NULL) {
            geo_set_animation_globals(&node->objNode->header.gfx.animInfo, hasAnimation);
        }

        geo_process_node_and_siblings(node->objNode->header.gfx.sharedChild);
        WORLD(gCurGraphNodeHeldObject) = NULL;
        WORLD(gCurrAnimType) = WORLD(gGeoTempState).type;
        WORLD(gCurrAnimEnabled) = WORLD(gGeoTempState).enabled;
        WORLD(gCurrAnimFrame) = WORLD(gGeoTempState).frame;
        WORLD(gCurrAnimTranslationMultiplier) = WORLD(gGeoTempState).translationMultiplier;
        WORLD(gCurrAnimAttribute) = WORLD(gGeoTempState).attribute;
        WORLD(gCurrAnimData) = WORLD(gGeoTempState).data;
        WORLD(gMatStackIndex)--;
    }

    if (node->fnNode.node.children != NULL) {
        geo_process_node_and_siblings(node->fnNode.node.children);
    }
}

/**
 * Processes the children of the given GraphNode if it has any
 */
void geo_try_process_children(struct GraphNode *node) {
    N64_STACK_FRAME(geo_try_process_children);
    if (node->children != NULL) {
        geo_process_node_and_siblings(node->children);
    }
}

/**
 * Process a generic geo node and its siblings.
 * The first argument is the start node, and all its siblings will
 * be iterated over.
 */
void geo_process_node_and_siblings(struct GraphNode *firstNode) {
    N64_STACK_FRAME(geo_process_node_and_siblings);
    s16 iterateChildren = TRUE;
    struct GraphNode *curGraphNode = firstNode;
    struct GraphNode *parent = curGraphNode->parent;

    // In the case of a switch node, exactly one of the children of the node is
    // processed instead of all children like usual
    if (parent != NULL) {
        iterateChildren = (parent->type != GRAPH_NODE_TYPE_SWITCH_CASE);
    }

    do {
        if (curGraphNode->flags & GRAPH_RENDER_ACTIVE) {
            if (curGraphNode->flags & GRAPH_RENDER_CHILDREN_FIRST) {
                geo_try_process_children(curGraphNode);
            } else {
                switch (curGraphNode->type) {
                    case GRAPH_NODE_TYPE_ORTHO_PROJECTION:
                        geo_process_ortho_projection((struct GraphNodeOrthoProjection *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_PERSPECTIVE:
                        geo_process_perspective((struct GraphNodePerspective *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_MASTER_LIST:
                        geo_process_master_list((struct GraphNodeMasterList *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_LEVEL_OF_DETAIL:
                        geo_process_level_of_detail((struct GraphNodeLevelOfDetail *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_SWITCH_CASE:
                        geo_process_switch((struct GraphNodeSwitchCase *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_CAMERA:
                        geo_process_camera((struct GraphNodeCamera *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_TRANSLATION_ROTATION:
                        geo_process_translation_rotation(
                            (struct GraphNodeTranslationRotation *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_TRANSLATION:
                        geo_process_translation((struct GraphNodeTranslation *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_ROTATION:
                        geo_process_rotation((struct GraphNodeRotation *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_OBJECT:
                        geo_process_object((struct Object *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_ANIMATED_PART:
                        geo_process_animated_part((struct GraphNodeAnimatedPart *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_BILLBOARD:
                        geo_process_billboard((struct GraphNodeBillboard *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_DISPLAY_LIST:
                        geo_process_display_list((struct GraphNodeDisplayList *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_SCALE:
                        geo_process_scale((struct GraphNodeScale *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_SHADOW:
                        geo_process_shadow((struct GraphNodeShadow *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_OBJECT_PARENT:
                        geo_process_object_parent((struct GraphNodeObjectParent *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_GENERATED_LIST:
                        geo_process_generated_list((struct GraphNodeGenerated *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_BACKGROUND:
                        geo_process_background((struct GraphNodeBackground *) curGraphNode);
                        break;
                    case GRAPH_NODE_TYPE_HELD_OBJ:
                        geo_process_held_object((struct GraphNodeHeldObject *) curGraphNode);
                        break;
                    default:
                        geo_try_process_children((struct GraphNode *) curGraphNode);
                        break;
                }
            }
        } else {
            if (curGraphNode->type == GRAPH_NODE_TYPE_OBJECT) {
                ((struct GraphNodeObject *) curGraphNode)->throwMatrix = NULL;
            }
        }
    } while (iterateChildren && (curGraphNode = curGraphNode->next) != firstNode);
}

/**
 * Process a root node. This is the entry point for processing the scene graph.
 * The root node itself sets up the viewport, then all its children are processed
 * to set up the projection and draw display lists.
 */
void geo_process_root(struct GraphNodeRoot *node, Vp *b, Vp *c, s32 clearColor) {
    N64_STACK_FRAME(geo_process_root);
    UNUSED u8 filler[4];

    if (node->node.flags & GRAPH_RENDER_ACTIVE && !SM64_DRAW) {
        // Library: the walk without the viewport, the display list heap and
        // the initial fixed point matrix.
        WORLD(gMatStackIndex) = 0;
        WORLD(gCurrAnimType) = 0;
        mtxf_identity(WORLD(gMatStack)[WORLD(gMatStackIndex)]);
        WORLD(gCurGraphNodeRoot) = node;
        if (node->node.children != NULL) {
            geo_process_node_and_siblings(node->node.children);
        }
        WORLD(gCurGraphNodeRoot) = NULL;
    } else if (node->node.flags & GRAPH_RENDER_ACTIVE) {
        Mtx *initialMatrix;
        Vp *viewport = alloc_display_list(sizeof(*viewport));

        WORLD(gDisplayListHeap) = alloc_only_pool_init(main_pool_available() - sizeof(struct AllocOnlyPool),
                                                MEMORY_POOL_LEFT);
        initialMatrix = alloc_display_list(sizeof(*initialMatrix));
        WORLD(gMatStackIndex) = 0;
        WORLD(gCurrAnimType) = 0;
        vec3s_set(viewport->vp.vtrans, node->x * 4, node->y * 4, 511);
        vec3s_set(viewport->vp.vscale, node->width * 4, node->height * 4, 511);
        if (b != NULL) {
            clear_framebuffer(clearColor);
            make_viewport_clip_rect(b);
            *viewport = *b;
        }

        else if (c != NULL) {
            clear_framebuffer(clearColor);
            make_viewport_clip_rect(c);
        }

        mtxf_identity(WORLD(gMatStack)[WORLD(gMatStackIndex)]);
        mtxf_to_mtx(initialMatrix, WORLD(gMatStack)[WORLD(gMatStackIndex)]);
        WORLD(gMatStackFixed)[WORLD(gMatStackIndex)] = initialMatrix;
        gSPViewport(WORLD(gDisplayListHead)++, VIRTUAL_TO_PHYSICAL(viewport));
        gSPMatrix(WORLD(gDisplayListHead)++, VIRTUAL_TO_PHYSICAL(WORLD(gMatStackFixed)[WORLD(gMatStackIndex)]),
                  G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
        WORLD(gCurGraphNodeRoot) = node;
        if (node->node.children != NULL) {
            geo_process_node_and_siblings(node->node.children);
        }
        WORLD(gCurGraphNodeRoot) = NULL;
        if (WORLD(gShowDebugText)) {
            print_text_fmt_int(180, 36, "MEM %d",
                               WORLD(gDisplayListHeap)->totalSpace - WORLD(gDisplayListHeap)->usedSpace);
        }
        main_pool_free(WORLD(gDisplayListHeap));
    }
}

// Library: its variables' addresses (tools/state/types.py).
#include "pointers/game/src/game/rendering_graph_node.c.inc.c"
