// Worlds: each a copy of the game's whole state (docs/state.md). This file is
// not part of the state; it keeps what the process shares: the state's
// layout and initial values, and each thread's current world and settings.
#include <ultra64.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <threads.h>

#include "sm64_physics.h"

// The game's state: every writable variable of the game and of the host's
// stand-ins for the console, linked as one section (platform/state.ld), page
// aligned at both ends. The variables that the N64 resets by loading their
// segment from ROM again are ranges of it.
extern char sm64_state_start[], sm64_state_end[];
extern char sm64_overlay_start[], sm64_overlay_bss[], sm64_overlay_end[];
extern char sm64_level_data_start[], sm64_level_data_bss[], sm64_level_data_end[];

// Offsets in the state of the initial values that are addresses in the state
// (tools/state/init_pointers.py, sorted).
extern const uint32_t gHostStatePointers[];
extern const size_t gHostStatePointerCount;

// platform/world.h: the current world's offset from the section.
__thread ptrdiff_t gHostWorldOffset __attribute__((tls_model("initial-exec")));

// Settings, per thread: whether the sound thread runs (platform/host.c) and
// whether the render walk draws (platform/draw.h).
__thread bool gHostRunAudio __attribute__((tls_model("initial-exec")));
__thread int gHostDraw __attribute__((tls_model("initial-exec")));

void host_boot(void); // platform/host.c
void host_step(uint32_t input);

struct sm64_world {
    char *memory;
};

// The state before power-on, with its addresses in the section. The section
// itself is never used once a world exists: it is made inaccessible, so code
// that reaches the state without WORLD() faults instead of sharing it.
static char *sInitial;
static once_flag sInitOnce = ONCE_FLAG_INIT;

static size_t state_size(void) {
    return sm64_state_end - sm64_state_start;
}

static void init_process(void) {
    sInitial = malloc(state_size());
    memcpy(sInitial, sm64_state_start, state_size());
    mprotect(sm64_state_start, state_size(), PROT_NONE);
}

static void init(void) {
    call_once(&sInitOnce, init_process);
}

// Copies the initial values of [start, end) of the section into the world at
// memory, with their addresses in the state moved there.
static void copy_initial(char *memory, const char *start, const char *end) {
    const size_t from = start - sm64_state_start, to = end - sm64_state_start;
    memcpy(memory + from, sInitial + from, to - from);
    const uintptr_t lo = (uintptr_t) sm64_state_start, hi = (uintptr_t) sm64_state_end;
    const intptr_t delta = memory - sm64_state_start;
    for (size_t i = 0; i < gHostStatePointerCount; ++i) {
        const size_t offset = gHostStatePointers[i];
        if (offset < from || offset >= to) {
            continue;
        }
        uintptr_t value;
        memcpy(&value, memory + offset, sizeof(value));
        if (value >= lo && value < hi) {
            value += delta;
            memcpy(memory + offset, &value, sizeof(value));
        }
    }
}

// The current world's memory.
static char *current(void) {
    return sm64_state_start + gHostWorldOffset;
}

// FIXED_LOAD (docs/changes.md 6): src/menu's and src/goddard's variables, read
// from ROM again.
void host_restore_overlay(void) {
    copy_initial(current(), sm64_overlay_start, sm64_overlay_bss);
    memset(current() + (sm64_overlay_bss - sm64_state_start), 0, sm64_overlay_end - sm64_overlay_bss);
}

// LOAD_MIO0 of segment 7 (docs/changes.md 8): the level's data, decompressed
// from ROM again. The other levels' data is not in use, so restoring all of it
// is the same.
void host_restore_level_data(void) {
    copy_initial(current(), sm64_level_data_start, sm64_level_data_bss);
    memset(current() + (sm64_level_data_bss - sm64_state_start), 0, sm64_level_data_end - sm64_level_data_bss);
}

// --- The ROM -------------------------------------------------------------------

bool host_load_rom(const void *rom, size_t size); // platform/host.c

bool sm64_load_rom(const void *rom, size_t size) {
    init();
    // What it loads becomes part of the initial values: the worlds created
    // from then on start with it.
    const ptrdiff_t offset = gHostWorldOffset;
    gHostWorldOffset = sInitial - sm64_state_start;
    const bool loaded = host_load_rom(rom, size);
    gHostWorldOffset = offset;
    return loaded;
}

// --- Worlds --------------------------------------------------------------------

sm64_world *sm64_world_create(void) {
    init();
    sm64_world *world = malloc(sizeof(*world));
    // The section's alignment, so every variable keeps its own.
    world->memory = aligned_alloc(65536, state_size());
    if (!world->memory) {
        free(world);
        return NULL;
    }
    copy_initial(world->memory, sm64_state_start, sm64_state_end);
    const ptrdiff_t offset = gHostWorldOffset;
    sm64_world_enter(world);
    host_boot();
    gHostWorldOffset = offset;
    return world;
}

void sm64_world_destroy(sm64_world *world) {
    if (world) {
        free(world->memory);
        free(world);
    }
}

void sm64_world_enter(const sm64_world *world) {
    gHostWorldOffset = world->memory - sm64_state_start;
}


void sm64_step(sm64_world *world, uint32_t input) {
    const ptrdiff_t offset = gHostWorldOffset;
    sm64_world_enter(world);
    host_step(input);
    gHostWorldOffset = offset;
}

size_t sm64_state_size(void) {
    return state_size();
}

void sm64_save_state(const sm64_world *world, void *buffer) {
    memcpy(buffer, world->memory, state_size());
}

void sm64_load_state(sm64_world *world, const void *buffer) {
    memcpy(world->memory, buffer, state_size());
}

// --- Settings --------------------------------------------------------------------

void sm64_set_audio(bool enabled) {
    gHostRunAudio = enabled;
}

void sm64_set_draw(bool enabled) {
    gHostDraw = enabled;
}
