// The world a thread is stepping (docs/state.md). Every variable of the game's
// state (the sm64_state section, platform/state.ld) exists once per world; the
// code reaches the current world's through WORLD(x), the variable's address
// moved by the current world's offset from the section.
#ifndef SM64_PLATFORM_WORLD_H
#define SM64_PLATFORM_WORLD_H

#include <stddef.h>

#include "pointers.h"

extern __thread ptrdiff_t gHostWorldOffset __attribute__((tls_model("initial-exec")));

#define WORLD(x) (*(__typeof__(&(x))) ((char *) &(x) + gHostWorldOffset))

#endif
