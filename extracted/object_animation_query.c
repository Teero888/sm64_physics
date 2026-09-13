/* Generated verbatim upstream function extraction. See extracted.json. */
#include "host/object_context.h"

#line 1000 "n64decomp/src/game/object_helpers.c"
s32 cur_obj_check_anim_frame(s32 frame) {
    s32 animFrame = o->header.gfx.animInfo.animFrame;

    if (animFrame == frame) {
        return TRUE;
    } else {
        return FALSE;
    }
}

#line 1010 "n64decomp/src/game/object_helpers.c"
s32 cur_obj_check_anim_frame_in_range(s32 startFrame, s32 rangeLength) {
    s32 animFrame = o->header.gfx.animInfo.animFrame;

    if (animFrame >= startFrame && animFrame < startFrame + rangeLength) {
        return TRUE;
    } else {
        return FALSE;
    }
}
