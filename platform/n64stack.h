// A model of the N64 game thread's stack pointer, for the places where the
// original lets a stack address leak into game state (docs/avoid_ub.md).
//
// The functions on the N64's call paths to those places open with
// N64_STACK_FRAME(name); (tools/n64stack/frames.py puts it there): entering
// one moves the modelled stack pointer down by its N64 frame, leaving it by
// any return moves it back up. The frames of the version being built come
// from n64_frames.h, generated from tools/n64stack/<version>.tsv; a function
// on the paths in another version only has a frame of 0 in this one.
#pragma once
#include <stdint.h>

#include "n64_frames.h"

// The modelled stack pointer inside the innermost function with a frame.
extern __thread uint32_t gN64StackPointer __attribute__((tls_model("initial-exec")));

static inline __attribute__((always_inline)) uint32_t n64stack_enter(uint32_t frame) {
    gN64StackPointer -= frame;
    return frame;
}

static inline __attribute__((always_inline)) void n64stack_leave(const uint32_t *frame) {
    gN64StackPointer += *frame;
}

#define N64_STACK_FRAME(function)                                                                      \
    const uint32_t n64stack_frame __attribute__((cleanup(n64stack_leave), unused)) =                    \
        n64stack_enter(N64_FRAME_##function)
