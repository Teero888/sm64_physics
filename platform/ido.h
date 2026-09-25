// How IDO, the compiler of the original game, converts floating point values
// to unsigned integers, where C leaves it undefined (negative or too large
// values) and x86 gives different results.
//
// From its code (e.g. bhv_piranha_plant_bubble_loop in the US build): with the
// FPU rounding toward zero it converts to a signed 32-bit integer with
// cvt.w.s/cvt.w.d. If that sets no exception flag, a negative result becomes
// 0xFFFFFFFF and any other is the result. If it does (the value is 2^31 or
// more, or out of range, or NaN), it subtracts 2^31 and converts again: the
// result with the top bit set if that works, 0xFFFFFFFF if not.
#ifndef IDO_H
#define IDO_H

#include <PR/ultratypes.h>

static inline u32 ido_f64_to_u32(f64 x) {
    // The first conversion succeeds for truncated values in [-2^31, 2^31).
    if (x > -2147483649.0 && x < 2147483648.0) {
        const s32 t = (s32) x;
        return t < 0 ? 0xFFFFFFFFu : (u32) t;
    }
    x -= 2147483648.0;
    if (x > -2147483649.0 && x < 2147483648.0) {
        const s32 t = (s32) x;
        return t < 0 ? 0xFFFFFFFFu : ((u32) t | 0x80000000u);
    }
    return 0xFFFFFFFFu; // out of range or NaN
}

static inline u32 ido_f32_to_u32(f32 x) {
    if (x > -2147483649.0f && x < 2147483648.0f) {
        const s32 t = (s32) x;
        return t < 0 ? 0xFFFFFFFFu : (u32) t;
    }
    x -= 2147483648.0f;
    if (x > -2147483649.0f && x < 2147483648.0f) {
        const s32 t = (s32) x;
        return t < 0 ? 0xFFFFFFFFu : ((u32) t | 0x80000000u);
    }
    return 0xFFFFFFFFu;
}

// (u16) x as IDO compiles it: floating point values go through the unsigned
// conversion above, integers are truncated.
#define IDO_U16(x)                                                                                     \
    _Generic((x), f32: (u16) ido_f32_to_u32((f32)(x)), f64: (u16) ido_f64_to_u32((f64)(x)), default: (u16)(x))

#endif
