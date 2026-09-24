// A model of the N64 game thread's stack pointer, for the places where the
// original lets a stack address leak into game state (docs/avoid_ub.md).
//
// The files on the call path to those places are compiled with
// -finstrument-functions through generated wrappers (tools/n64stack), which
// register each function's N64 frame size. Entering a function moves the
// modelled stack pointer down by its frame, returning moves it back up.
#pragma once
#include <stdint.h>

struct n64_frame {
    const void *function;
    uint32_t size;
};

// The modelled stack pointer inside the innermost instrumented function.
extern uint32_t gN64StackPointer;

void n64stack_register(const struct n64_frame *frames, unsigned count);

#define N64STACK_REGISTER(name, table)                                                                  \
    __attribute__((constructor, no_instrument_function)) static void n64stack_register_##name(void) {    \
        n64stack_register(table, sizeof(table) / sizeof(table[0]));                                      \
    }
