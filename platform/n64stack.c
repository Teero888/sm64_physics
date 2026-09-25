#include "n64stack.h"

#include <stdlib.h>

// Per thread: it only lives during a step.
__thread uint32_t gN64StackPointer __attribute__((tls_model("initial-exec")));

// Function address -> N64 frame size, open addressing. Every instrumented
// call looks up here twice, so it has to be cheap.
#define TABLE_BITS 12
#define TABLE_SIZE (1u << TABLE_BITS)
static struct n64_frame sTable[TABLE_SIZE];

__attribute__((no_instrument_function)) static unsigned slot(const void *function) {
    uintptr_t a = (uintptr_t) function;
    return (unsigned) ((a >> 4) ^ (a >> (4 + TABLE_BITS))) & (TABLE_SIZE - 1);
}

__attribute__((no_instrument_function)) void n64stack_register(const struct n64_frame *frames, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        unsigned s = slot(frames[i].function);
        while (sTable[s].function != NULL && sTable[s].function != frames[i].function) {
            s = (s + 1) & (TABLE_SIZE - 1);
        }
        sTable[s] = frames[i];
    }
}

__attribute__((no_instrument_function)) static uint32_t frame_of(const void *function) {
    for (unsigned s = slot(function);; s = (s + 1) & (TABLE_SIZE - 1)) {
        if (sTable[s].function == function) {
            return sTable[s].size;
        }
        if (sTable[s].function == NULL) {
            // Not on the N64 (a helper that only exists natively): no frame.
            return 0;
        }
    }
}

__attribute__((no_instrument_function)) void __cyg_profile_func_enter(void *function, void *call_site) {
    (void) call_site;
    gN64StackPointer -= frame_of(function);
}

__attribute__((no_instrument_function)) void __cyg_profile_func_exit(void *function, void *call_site) {
    (void) call_site;
    gN64StackPointer += frame_of(function);
}
