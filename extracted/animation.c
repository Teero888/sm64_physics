/* Generated verbatim upstream function extraction. See extracted.json. */
#include "host/animation_state.h"

#line 736 "n64decomp/src/engine/graph_node.c"
void geo_obj_init_animation(struct GraphNodeObject *graphNode, struct Animation **animPtrAddr) {
    struct Animation **animSegmented = segmented_to_virtual(animPtrAddr);
    struct Animation *anim = segmented_to_virtual(*animSegmented);

    if (graphNode->animInfo.curAnim != anim) {
        graphNode->animInfo.curAnim = anim;
        graphNode->animInfo.animFrame = anim->startFrame + ((anim->flags & ANIM_FLAG_BACKWARD) ? 1 : -1);
        graphNode->animInfo.animAccel = 0;
        graphNode->animInfo.animYTrans = 0;
    }
}

#line 751 "n64decomp/src/engine/graph_node.c"
void geo_obj_init_animation_accel(struct GraphNodeObject *graphNode, struct Animation **animPtrAddr, u32 animAccel) {
    struct Animation **animSegmented = segmented_to_virtual(animPtrAddr);
    struct Animation *anim = segmented_to_virtual(*animSegmented);

    if (graphNode->animInfo.curAnim != anim) {
        graphNode->animInfo.curAnim = anim;
        graphNode->animInfo.animYTrans = 0;
        graphNode->animInfo.animFrameAccelAssist =
            (anim->startFrame << 16) + ((anim->flags & ANIM_FLAG_BACKWARD) ? animAccel : -animAccel);
        graphNode->animInfo.animFrame = graphNode->animInfo.animFrameAccelAssist >> 16;
    }

    graphNode->animInfo.animAccel = animAccel;
}

#line 773 "n64decomp/src/engine/graph_node.c"
s32 retrieve_animation_index(s32 frame, u16 **attributes) {
    s32 result;

    if (frame < (*attributes)[0]) {
        result = (*attributes)[1] + frame;
    } else {
        result = (*attributes)[1] + (*attributes)[0] - 1;
    }

    *attributes += 2;

    return result;
}

#line 792 "n64decomp/src/engine/graph_node.c"
s16 geo_update_animation_frame(struct AnimInfo *obj, s32 *accelAssist) {
    s32 result;
    struct Animation *anim = obj->curAnim;

    if (obj->animTimer == gAreaUpdateCounter || anim->flags & ANIM_FLAG_2) {
        if (accelAssist != NULL) {
            accelAssist[0] = obj->animFrameAccelAssist;
        }

        return obj->animFrame;
    }

    if (anim->flags & ANIM_FLAG_BACKWARD) {
        if (obj->animAccel != 0) {
            result = obj->animFrameAccelAssist - obj->animAccel;
        } else {
            result = (obj->animFrame - 1) << 16;
        }

        if (GET_HIGH_S16_OF_32(result) < anim->loopStart) {
            if (anim->flags & ANIM_FLAG_NOLOOP) {
                SET_HIGH_S16_OF_32(result, anim->loopStart);
            } else {
                SET_HIGH_S16_OF_32(result, anim->loopEnd - 1);
            }
        }
    } else {
        if (obj->animAccel != 0) {
            result = obj->animFrameAccelAssist + obj->animAccel;
        } else {
            result = (obj->animFrame + 1) << 16;
        }

        if (GET_HIGH_S16_OF_32(result) >= anim->loopEnd) {
            if (anim->flags & ANIM_FLAG_NOLOOP) {
                SET_HIGH_S16_OF_32(result, anim->loopEnd - 1);
            } else {
                SET_HIGH_S16_OF_32(result, anim->loopStart);
            }
        }
    }

    if (accelAssist != 0) {
        accelAssist[0] = result;
    }

    return GET_HIGH_S16_OF_32(result);
}
