// Worlds: each a copy of the game's whole state (docs/state.md). This file is
// not part of the state; it keeps what the process shares: the state's
// layout and initial values, and each thread's current world and settings.
#include <ultra64.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "sm64_physics.h"
#include "pointers.h"
#include "os.h"
#include "rom.h"

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
__thread const void *gHostDrawnList __attribute__((tls_model("initial-exec")));

void host_boot(void); // platform/host.c
void host_step(uint32_t input);

// The pointer map (platform/pointers.h): bit i of a world's map says that an
// address may be kept at byte 4 * i of its memory.
typedef uint32_t map_word;
#define MAP_BITS 32

struct sm64_world {
    char *memory;
    map_word *map;
    // SM64_NOTE_ALLOCATIONS: who allocated each 4 bytes (world.c).
    uint32_t *owners;
};

// The current world's map, per thread.
static __thread map_word *tCurrentMap __attribute__((tls_model("initial-exec")));

static const struct host_field sAddressField = { 0, NULL, 1, 0 };
const struct host_type gHostTypeAddress = { sizeof(void *), 1, &sAddressField };

// The variables that keep addresses (host_register_variables), and the map
// of the state's initial values made from them.
static struct host_variable *sVariables;
static size_t sVariableCount, sVariableCapacity;
static map_word *sInitialMap;

// The state before power-on, with its addresses in the section. The section
// itself is never used once a world exists: it is made inaccessible, so code
// that reaches the state without WORLD() faults instead of sharing it.
static char *sInitial;
static pthread_once_t sInitOnce = PTHREAD_ONCE_INIT;

static size_t state_size(void) {
    return sm64_state_end - sm64_state_start;
}

static size_t map_size(void) {
    return (state_size() / 4 + MAP_BITS - 1) / MAP_BITS * sizeof(map_word);
}

void host_register_variables(const struct host_variable *variables, size_t count) {
    if (sVariableCount + count > sVariableCapacity) {
        sVariableCapacity = (sVariableCount + count) * 2;
        sVariables = realloc(sVariables, sVariableCapacity * sizeof(*sVariables));
    }
    memcpy(sVariables + sVariableCount, variables, count * sizeof(*variables));
    sVariableCount += count;
}

// --- Marking -------------------------------------------------------------------

static void map_clear(map_word *map, size_t from, size_t to) {
    size_t i = from / 4;
    const size_t end = (to + 3) / 4;
    // Bit by bit up to a word, whole words, the rest bit by bit.
    for (; i < end && i % MAP_BITS; ++i) {
        map[i / MAP_BITS] &= ~((map_word) 1 << (i % MAP_BITS));
    }
    if (end - i >= MAP_BITS) {
        const size_t words = (end - i) / MAP_BITS;
        memset(&map[i / MAP_BITS], 0, words * sizeof(map_word));
        i += words * MAP_BITS;
    }
    for (; i < end; ++i) {
        map[i / MAP_BITS] &= ~((map_word) 1 << (i % MAP_BITS));
    }
}

static void map_set(map_word *map, size_t offset) {
    // Addresses the game keeps in its memory are at least 4-byte aligned.
    const size_t i = offset / 4;
    map[i / MAP_BITS] |= (map_word) 1 << (i % MAP_BITS);
}

static void map_type(map_word *map, size_t offset, const struct host_type *type, size_t count) {
    for (size_t e = 0; e < count; ++e, offset += type->size) {
        for (size_t f = 0; f < type->field_count; ++f) {
            const struct host_field *field = &type->fields[f];
            for (size_t k = 0; k < field->count; ++k) {
                const size_t at = offset + field->offset + k * field->stride;
                if (field->type) {
                    map_type(map, at, field->type, 1);
                } else {
                    map_set(map, at);
                }
            }
        }
    }
}

static size_t current_offset(const void *p) {
    return (const char *) p - (sm64_state_start + gHostWorldOffset);
}

void host_mark_raw(void *p, size_t size) {
    const size_t offset = current_offset(p);
    map_clear(tCurrentMap, offset, offset + size);
}

void host_mark(void *p, const struct host_type *type, size_t count) {
    const size_t offset = current_offset(p);
    map_clear(tCurrentMap, offset, offset + type->size * count);
    map_type(tCurrentMap, offset, type, count);
}

void host_mark_address(void *p) {
    map_set(tCurrentMap, current_offset(p));
}

// --- Allocation notes (debugging) ---------------------------------------------
// With SM64_NOTE_ALLOCATIONS set, every world keeps, for each 4 bytes of its
// memory, which code allocated what is there (an index into sCallers).

static int sNoteAllocations = -1;
static pthread_mutex_t sCallerLock;
static void **sCallers;
static uint32_t sCallerCount, sCallerCapacity;
static __thread uint32_t *tCurrentOwners __attribute__((tls_model("initial-exec")));

static bool noting(void) {
    if (sNoteAllocations < 0) {
        sNoteAllocations = getenv("SM64_NOTE_ALLOCATIONS") != NULL;
        pthread_mutex_init(&sCallerLock, NULL);
    }
    return sNoteAllocations > 0;
}

static uint32_t caller_index(void *caller) {
    pthread_mutex_lock(&sCallerLock);
    uint32_t i = 0;
    while (i < sCallerCount && sCallers[i] != caller) {
        ++i;
    }
    if (i == sCallerCount) {
        if (sCallerCount == sCallerCapacity) {
            sCallerCapacity = sCallerCapacity ? sCallerCapacity * 2 : 256;
            sCallers = realloc(sCallers, sCallerCapacity * sizeof(*sCallers));
        }
        sCallers[sCallerCount++] = caller;
    }
    pthread_mutex_unlock(&sCallerLock);
    return i + 1;
}

static void note_range(size_t from, size_t to, uint32_t owner) {
    if (!tCurrentOwners) {
        return;
    }
    for (size_t i = from / 4; i < (to + 3) / 4 && i < state_size() / 4; ++i) {
        tCurrentOwners[i] = owner;
    }
}

void host_note_allocation(void *p, size_t size, void *caller) {
    if (noting()) {
        note_range(current_offset(p), current_offset(p) + size, caller_index(caller));
    }
}

void host_note_release(const void *start, const void *end) {
    if (noting()) {
        note_range(current_offset(start), current_offset(end), 0);
    }
}



// Moves the addresses at the map's places that point into [from, from + size)
// by delta.
static void relocate(char *memory, const map_word *map, uintptr_t from, intptr_t delta) {
    const size_t words = map_size() / sizeof(map_word);
    const uintptr_t to = from + state_size();
    for (size_t w = 0; w < words; ++w) {
        map_word bits = map[w];
        while (bits) {
            const int b = __builtin_ctz(bits);
            bits &= bits - 1;
            const size_t offset = (w * MAP_BITS + b) * 4;
            if (offset + sizeof(uintptr_t) > state_size()) {
                continue;
            }
            uintptr_t value;
            memcpy(&value, memory + offset, sizeof(value));
            if (value >= from && value < to) {
                value += delta;
                memcpy(memory + offset, &value, sizeof(value));
            }
        }
    }
}

static void init_process(void) {
    sInitial = malloc(state_size());
    memcpy(sInitial, sm64_state_start, state_size());
    sInitialMap = calloc(1, map_size());
    for (size_t i = 0; i < sVariableCount; ++i) {
        const struct host_variable *v = &sVariables[i];
        map_type(sInitialMap, (const char *) v->address - sm64_state_start, v->type, v->count);
    }
    os_protect_none(sm64_state_start, state_size());
}

static void init(void) {
    pthread_once(&sInitOnce, init_process);
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

// The initial pointer map of [start, end).
static void copy_initial_map(map_word *map, const char *start, const char *end) {
    const size_t from = start - sm64_state_start, to = end - sm64_state_start;
    map_clear(map, from, to);
    for (size_t i = from / 4; i < to / 4; ++i) {
        if (sInitialMap[i / MAP_BITS] >> (i % MAP_BITS) & 1) {
            map_set(map, i * 4);
        }
    }
}

// FIXED_LOAD (docs/changes.md 6): src/menu's and src/goddard's variables, read
// from ROM again.
void host_restore_overlay(void) {
    copy_initial(current(), sm64_overlay_start, sm64_overlay_bss);
    memset(current() + (sm64_overlay_bss - sm64_state_start), 0, sm64_overlay_end - sm64_overlay_bss);
    copy_initial_map(tCurrentMap, sm64_overlay_start, sm64_overlay_end);
}

// LOAD_MIO0 of segment 7 (docs/changes.md 8): the level's data, decompressed
// from ROM again. The other levels' data is not in use, so restoring all of it
// is the same.
void host_restore_level_data(void) {
    copy_initial(current(), sm64_level_data_start, sm64_level_data_bss);
    memset(current() + (sm64_level_data_bss - sm64_state_start), 0, sm64_level_data_end - sm64_level_data_bss);
    copy_initial_map(tCurrentMap, sm64_level_data_start, sm64_level_data_end);
}

// --- The ROM -------------------------------------------------------------------

void host_load_rom(const unsigned char *rom); // platform/host.c

bool sm64_load_rom(const void *data, size_t size) {
    init();
    unsigned char *rom = rom_normalize(data, size);
    if (!rom) {
        return false;
    }
    // What it loads becomes part of the initial values: the worlds created
    // from then on start with it.
    const ptrdiff_t offset = gHostWorldOffset;
    gHostWorldOffset = sInitial - sm64_state_start;
    host_load_rom(rom);
    gHostWorldOffset = offset;
    rom_keep(rom, size);
    return true;
}

// --- Worlds --------------------------------------------------------------------

static sm64_world *world_new(void) {
    sm64_world *world = malloc(sizeof(*world));
    // The section's alignment, so every variable keeps its own.
    world->memory = os_aligned_alloc(65536, state_size());
    world->map = malloc(map_size());
    world->owners = noting() ? calloc(state_size() / 4, sizeof(uint32_t)) : NULL;
    if (!world->memory || !world->map) {
        os_aligned_free(world->memory);
        free(world->map);
        free(world);
        return NULL;
    }
    return world;
}

sm64_world *sm64_world_create(void) {
    init();
    sm64_world *world = world_new();
    if (!world) {
        return NULL;
    }
    copy_initial(world->memory, sm64_state_start, sm64_state_end);
    memcpy(world->map, sInitialMap, map_size());
    const ptrdiff_t offset = gHostWorldOffset;
    sm64_world_enter(world);
    host_boot();
    gHostWorldOffset = offset;
    return world;
}

void sm64_world_destroy(sm64_world *world) {
    if (world) {
        os_aligned_free(world->memory);
        free(world->map);
        free(world->owners);
        free(world);
    }
}

void sm64_world_enter(const sm64_world *world) {
    gHostWorldOffset = world->memory - sm64_state_start;
    tCurrentMap = world->map;
    tCurrentOwners = world->owners;
}

// The code that allocated what is at offset of world's memory, or NULL
// (SM64_NOTE_ALLOCATIONS).
void *sm64_world_allocation_at(const void *opaque, size_t offset) {
    const sm64_world *world = opaque;
    const uint32_t owner = world->owners ? world->owners[offset / 4] : 0;
    return owner ? sCallers[owner - 1] : NULL;
}

void sm64_world_copy(sm64_world *dst, const sm64_world *src) {
    if (dst == src) {
        return;
    }
    memcpy(dst->memory, src->memory, state_size());
    memcpy(dst->map, src->map, map_size());
    relocate(dst->memory, dst->map, (uintptr_t) src->memory, dst->memory - src->memory);
}

sm64_world *sm64_world_clone(const sm64_world *src) {
    sm64_world *world = world_new();
    if (world) {
        sm64_world_copy(world, src);
    }
    return world;
}


void sm64_step(sm64_world *world, uint32_t input) {
    const ptrdiff_t offset = gHostWorldOffset;
    map_word *map = tCurrentMap;
    uint32_t *owners = tCurrentOwners;
    sm64_world_enter(world);
    host_step(input);
    gHostWorldOffset = offset;
    tCurrentMap = map;
    tCurrentOwners = owners;
}

const void *sm64_step_draw(sm64_world *world, uint32_t input) {
    const int draw = gHostDraw;
    gHostDraw = 1;
    gHostDrawnList = NULL;
    sm64_step(world, input);
    gHostDraw = draw;
    return gHostDrawnList;
}

// A saved state: where its world's memory was, the memory, the pointer map.
struct saved_header {
    uint64_t magic, memory;
};
#define SAVED_MAGIC 0x736d363473746174ull // "sm64stat"

size_t sm64_state_size(void) {
    return sizeof(struct saved_header) + state_size() + map_size();
}

void sm64_save_state(const sm64_world *world, void *buffer) {
    struct saved_header header = { SAVED_MAGIC, (uintptr_t) world->memory };
    memcpy(buffer, &header, sizeof(header));
    memcpy((char *) buffer + sizeof(header), world->memory, state_size());
    memcpy((char *) buffer + sizeof(header) + state_size(), world->map, map_size());
}

bool sm64_load_state(sm64_world *world, const void *buffer) {
    struct saved_header header;
    memcpy(&header, buffer, sizeof(header));
    if (header.magic != SAVED_MAGIC) {
        return false;
    }
    memcpy(world->memory, (const char *) buffer + sizeof(header), state_size());
    memcpy(world->map, (const char *) buffer + sizeof(header) + state_size(), map_size());
    if (header.memory != (uintptr_t) world->memory) {
        relocate(world->memory, world->map, header.memory, (uintptr_t) world->memory - header.memory);
    }
    return true;
}

// --- Settings --------------------------------------------------------------------

void sm64_set_audio(bool enabled) {
    gHostRunAudio = enabled;
}

void sm64_set_draw(bool enabled) {
    gHostDraw = enabled;
}
