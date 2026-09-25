// The world a thread is stepping (docs/state.md). Every variable of the game's
// state (the sm64_state section, platform/state.ld) exists once per world; the
// code reaches the current world's through WORLD(x), the variable's address
// moved by the current world's offset from the section.
#ifndef SM64_PLATFORM_WORLD_H
#define SM64_PLATFORM_WORLD_H

#include <stddef.h>

#include "pointers.h"

extern __thread ptrdiff_t gHostWorldOffset __attribute__((tls_model("initial-exec")));

// The address goes through an empty asm: the compiler must not know which
// variable the result belongs to. It would otherwise take the moved address
// for an address inside the variable itself, and could fold reads of a
// variable it sees no writes to into its initial value, or (Clang) turn such
// a variable into a constant outside the state section.
static inline __attribute__((always_inline)) char *host_world_address(char *address) {
    __asm__("" : "+r"(address));
    return address + gHostWorldOffset;
}

#define WORLD(x) (*(__typeof__(&(x))) host_world_address((char *) &(x)))

#endif
