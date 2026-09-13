/* Generated verbatim upstream function extraction. See extracted.json. */
#include "sm64.h"

#line 2147 "n64decomp/src/game/object_helpers.c"
f32 absf(f32 x) {
    if (x >= 0) {
        return x;
    } else {
        return -x;
    }
}

#line 2155 "n64decomp/src/game/object_helpers.c"
s32 absi(s32 x) {
    if (x >= 0) {
        return x;
    } else {
        return -x;
    }
}

#line 653 "n64decomp/src/game/object_helpers.c"
void linear_mtxf_mul_vec3f(Mat4 m, Vec3f dst, Vec3f v) {
    s32 i;
    for (i = 0; i < 3; i++) {
        dst[i] = m[0][i] * v[0] + m[1][i] * v[1] + m[2][i] * v[2];
    }
}

#line 668 "n64decomp/src/game/object_helpers.c"
void linear_mtxf_transpose_mul_vec3f(Mat4 m, Vec3f dst, Vec3f v) {
    s32 i;
    for (i = 0; i < 3; i++) {
        dst[i] = m[i][0] * v[0] + m[i][1] * v[1] + m[i][2] * v[2];
    }
}
