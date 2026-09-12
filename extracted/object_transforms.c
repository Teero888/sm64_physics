/* Generated verbatim upstream function extraction. See extracted.json. */
#include "sm64.h"
#include "engine/math_util.h"
#include "game/object_helpers.h"

#line 216 "n64decomp/src/game/object_helpers.c"
void obj_apply_scale_to_matrix(struct Object *obj, Mat4 dst, Mat4 src) {
    dst[0][0] = src[0][0] * obj->header.gfx.scale[0];
    dst[1][0] = src[1][0] * obj->header.gfx.scale[1];
    dst[2][0] = src[2][0] * obj->header.gfx.scale[2];
    dst[3][0] = src[3][0];

    dst[0][1] = src[0][1] * obj->header.gfx.scale[0];
    dst[1][1] = src[1][1] * obj->header.gfx.scale[1];
    dst[2][1] = src[2][1] * obj->header.gfx.scale[2];
    dst[3][1] = src[3][1];

    dst[0][2] = src[0][2] * obj->header.gfx.scale[0];
    dst[1][2] = src[1][2] * obj->header.gfx.scale[1];
    dst[2][2] = src[2][2] * obj->header.gfx.scale[2];
    dst[3][2] = src[3][2];

    dst[0][3] = src[0][3];
    dst[1][3] = src[1][3];
    dst[2][3] = src[2][3];
    dst[3][3] = src[3][3];
}

#line 295 "n64decomp/src/game/object_helpers.c"
f32 dist_between_objects(struct Object *obj1, struct Object *obj2) {
    f32 dx = obj1->oPosX - obj2->oPosX;
    f32 dy = obj1->oPosY - obj2->oPosY;
    f32 dz = obj1->oPosZ - obj2->oPosZ;

    return sqrtf(dx * dx + dy * dy + dz * dz);
}

#line 1908 "n64decomp/src/game/object_helpers.c"
void obj_build_transform_from_pos_and_angle(struct Object *obj, s16 posIndex, s16 angleIndex) {
    f32 translate[3];
    s16 rotation[3];

    translate[0] = obj->rawData.asF32[posIndex + 0];
    translate[1] = obj->rawData.asF32[posIndex + 1];
    translate[2] = obj->rawData.asF32[posIndex + 2];

    rotation[0] = obj->rawData.asS32[angleIndex + 0];
    rotation[1] = obj->rawData.asS32[angleIndex + 1];
    rotation[2] = obj->rawData.asS32[angleIndex + 2];

    mtxf_rotate_zxy_and_translate(obj->transform, translate, rotation);
}
