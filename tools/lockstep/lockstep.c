// Lockstep comparison: linked into the oracle frontend (oracle/oracle.c with
// SM64_LOCKSTEP), it steps the native game alongside the emulator and, at
// every poll, compares the game state in the emulator's RAM with the native
// state, member by member, through the layouts in layout_tables.h.
//
// Pointers cannot be compared by value. Both sides are reduced to what they
// point at: an object slot, or a symbol and the position in it (behavior
// scripts through the segment the N64 loaded them into). Pointers into memory
// allocated at run time only have to agree on being null or not.
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ultra64.h>
#include "types.h"
#include "game/area.h"
#include "game/camera.h"
#include "game/level_update.h"
#include "game/save_file.h"

#include "compare.h"
#include "elf_symbols.h"
#include "layout_tables.h"
#include "sm64_physics.h"

#define OBJECT_POOL_CAPACITY 240
#define MAX_REPORTED 40

// --- N64 RAM -------------------------------------------------------------------

static const uint8_t *sRam;

static uint32_t n64_u8(uint32_t a) {
    return sRam[(a & 0x7fffff) ^ 3];
}
static uint32_t n64_u16(uint32_t a) {
    return n64_u8(a) << 8 | n64_u8(a + 1);
}
static uint32_t n64_u32(uint32_t a) {
    return n64_u16(a) << 16 | n64_u16(a + 2);
}
static uint64_t n64_u64(uint32_t a) {
    return (uint64_t) n64_u32(a) << 32 | n64_u32(a + 4);
}

// --- N64 symbols -------------------------------------------------------------------

typedef struct {
    uint32_t address, size;
    char name[80];
} n64_symbol;

typedef struct {
    n64_symbol *symbols;
    size_t count;
} n64_table;

static n64_table sKseg0, sSegmented;

static int by_address(const void *a, const void *b) {
    const n64_symbol *x = a, *y = b;
    return x->address < y->address ? -1 : x->address > y->address;
}

static void load_table(n64_table *table, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "lockstep: cannot read %s\n", path);
        exit(1);
    }
    size_t capacity = 8192;
    table->symbols = malloc(capacity * sizeof(n64_symbol));
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        n64_symbol s;
        unsigned long address, size;
        if (sscanf(line, "%79s %lx %lu", s.name, &address, &size) != 3) {
            continue;
        }
        s.address = (uint32_t) address;
        s.size = (uint32_t) size;
        if (table->count == capacity) {
            capacity *= 2;
            table->symbols = realloc(table->symbols, capacity * sizeof(n64_symbol));
        }
        table->symbols[table->count++] = s;
    }
    fclose(f);
    qsort(table->symbols, table->count, sizeof(n64_symbol), by_address);
}

static const n64_symbol *n64_lookup(const char *name) {
    for (size_t i = 0; i < sKseg0.count; ++i) {
        const char *base = strrchr(sKseg0.symbols[i].name, ':');
        if (strcmp(base ? base + 1 : sKseg0.symbols[i].name, name) == 0) {
            return &sKseg0.symbols[i];
        }
    }
    fprintf(stderr, "lockstep: %s is not in the N64 symbol table\n", name);
    exit(1);
}

static const n64_symbol *n64_containing(const n64_table *table, uint32_t a) {
    size_t low = 0, high = table->count;
    while (low < high) {
        size_t mid = (low + high) / 2;
        if (table->symbols[mid].address <= a) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    if (low == 0) {
        return NULL;
    }
    const n64_symbol *s = &table->symbols[low - 1];
    return a < s->address + (s->size ? s->size : 1) ? s : NULL;
}

// --- Pointers ---------------------------------------------------------------------

enum { TARGET_NULL, TARGET_OBJECT, TARGET_SYMBOL, TARGET_OTHER };

typedef struct {
    int type;
    int index;           // object slot
    size_t offset, size; // position in the object or symbol
    const char *name;
} target;

static uint32_t sN64ObjectPool, sN64SegmentTable;
// The main pool (include/segments.h): level data, graph nodes, surfaces. The
// Mario head's code is loaded into its top for the title screen, so symbols
// there are not what the pool holds later.
#define N64_POOL_START 0x8005C000u
#define N64_POOL_END (0x8005C000u + 0x165000u)
static struct Object *sNativeObjectPool;

// Segments whose symbols are unique across the game: behavior scripts.
static const int sNamedSegments[] = { 0x13 };
static uint32_t sNamedSegmentSize[sizeof(sNamedSegments) / sizeof(sNamedSegments[0])];

// A segment's extent: the end of its last symbol. Other segments are loaded
// right after it in the pool.
static uint32_t segment_size(int segment) {
    uint32_t end = 0;
    for (size_t i = 0; i < sSegmented.count; ++i) {
        const n64_symbol *s = &sSegmented.symbols[i];
        if ((s->address >> 24) == (uint32_t) segment && (s->address & 0xffffff) + s->size > end) {
            end = (s->address & 0xffffff) + s->size;
        }
    }
    return end;
}

static target n64_target(uint32_t a) {
    target t = { TARGET_OTHER, 0, 0, 0, NULL };
    if (a == 0) {
        t.type = TARGET_NULL;
        return t;
    }
    if (a >= sN64ObjectPool && a < sN64ObjectPool + OBJECT_POOL_CAPACITY * N64_SIZEOF_Object) {
        t.type = TARGET_OBJECT;
        t.index = (a - sN64ObjectPool) / N64_SIZEOF_Object;
        t.offset = (a - sN64ObjectPool) % N64_SIZEOF_Object;
        return t;
    }
    const bool pooled = a >= N64_POOL_START && a < N64_POOL_END;
    const n64_symbol *s = pooled ? NULL : n64_containing(&sKseg0, a);
    if (pooled) {
        for (size_t i = 0; i < sizeof(sNamedSegments) / sizeof(sNamedSegments[0]); ++i) {
            const int segment = sNamedSegments[i];
            const uint32_t base = n64_u32(sN64SegmentTable + 4 * segment) | 0x80000000;
            if (a >= base && a - base < sNamedSegmentSize[i]) {
                s = n64_containing(&sSegmented, (uint32_t) segment << 24 | (a - base));
                if (s) {
                    a = (uint32_t) segment << 24 | (a - base);
                    break;
                }
            }
        }
    }
    if (s) {
        const char *base = strrchr(s->name, ':');
        t.type = TARGET_SYMBOL;
        t.name = base ? base + 1 : s->name;
        t.offset = a - s->address;
        t.size = s->size;
    }
    return t;
}

static target native_target(const void *p) {
    target t = { TARGET_OTHER, 0, 0, 0, NULL };
    if (p == NULL) {
        t.type = TARGET_NULL;
        return t;
    }
    const uintptr_t a = (uintptr_t) p, pool = (uintptr_t) sNativeObjectPool;
    if (a >= pool && a < pool + OBJECT_POOL_CAPACITY * sizeof(struct Object)) {
        t.type = TARGET_OBJECT;
        t.index = (a - pool) / sizeof(struct Object);
        t.offset = (a - pool) % sizeof(struct Object);
        return t;
    }
    const char *name = elf_symbol_containing(p, &t.offset, &t.size);
    if (name) {
        t.type = TARGET_SYMBOL;
        t.name = name;
    }
    return t;
}

static bool same_target(target n64, target native) {
    if (n64.type == TARGET_NULL || native.type == TARGET_NULL) {
        return n64.type == native.type;
    }
    if (n64.type == TARGET_OBJECT || native.type == TARGET_OBJECT) {
        return n64.type == native.type && n64.index == native.index && (n64.offset == 0) == (native.offset == 0);
    }
    if (n64.type == TARGET_SYMBOL && native.type == TARGET_SYMBOL) {
        // The same place in the same symbol: the same offset (a member laid
        // out alike) or the same fraction of it (an element of an array).
        return strcmp(n64.name, native.name) == 0
               && (n64.offset == native.offset
                   || (uint64_t) n64.offset * native.size == (uint64_t) native.offset * n64.size);
    }
    // Memory allocated at run time: only nullness is comparable.
    return true;
}

static void describe(char *out, size_t size, target t) {
    switch (t.type) {
        case TARGET_NULL: snprintf(out, size, "NULL"); break;
        case TARGET_OBJECT: snprintf(out, size, "object %d+%#zx", t.index, t.offset); break;
        case TARGET_SYMBOL: snprintf(out, size, "%s+%#zx/%#zx", t.name, t.offset, t.size); break;
        default: snprintf(out, size, "(allocated)"); break;
    }
}

// --- Comparison -------------------------------------------------------------------

static int sReported;
static uint32_t sPoll;

static void report(const char *format, ...) {
    if (sReported++ == 0) {
        printf("lockstep: first difference at poll %u (native frame %u)\n", sPoll, sPoll - 1);
    }
    if (sReported > MAX_REPORTED) {
        return;
    }
    va_list args;
    va_start(args, format);
    printf("  ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

static void compare_value(const char *where, const char *path, uint32_t index, int kind, uint32_t n64_address,
                          const uint8_t *native) {
    uint64_t expected = 0, actual = 0;
    switch (kind) {
        case LEAF_S8: case LEAF_U8: expected = n64_u8(n64_address); actual = *native; break;
        case LEAF_S16: case LEAF_U16: expected = n64_u16(n64_address); actual = *(const uint16_t *) native; break;
        case LEAF_S32: case LEAF_U32: case LEAF_F32:
            expected = n64_u32(n64_address);
            actual = *(const uint32_t *) native;
            break;
        case LEAF_S64: case LEAF_U64: case LEAF_F64:
            expected = n64_u64(n64_address);
            actual = *(const uint64_t *) native;
            break;
        case LEAF_PTR: {
            const target n64 = n64_target(n64_u32(n64_address));
            const target mine = native_target(*(void *const *) native);
            if (!same_target(n64, mine)) {
                char a[128], b[128];
                describe(a, sizeof(a), n64);
                describe(b, sizeof(b), mine);
                report("%s.%s[%u]: %s -> %s", where, path, index, a, b);
            }
            return;
        }
    }
    if (expected != actual) {
        if (kind == LEAF_F32) {
            float e, f;
            uint32_t eb = (uint32_t) expected, ab = (uint32_t) actual;
            memcpy(&e, &eb, 4);
            memcpy(&f, &ab, 4);
            report("%s.%s[%u]: %.9g (%08x) -> %.9g (%08x)", where, path, index, e, eb, f, ab);
        } else {
            report("%s.%s[%u]: %#llx -> %#llx", where, path, index, (unsigned long long) expected,
                   (unsigned long long) actual);
        }
    }
}

static void compare_struct(const char *where, const struct leaf *layout, uint32_t n64_base, const uint8_t *native) {
    for (const struct leaf *l = layout; l->path; ++l) {
        for (uint32_t i = 0; i < l->count; ++i) {
            compare_value(where, l->path, i, l->kind, n64_base + l->n64_offset + i * l->n64_stride,
                          native + l->native_offset + i * l->native_stride);
        }
    }
}

// Objects: natively, fields used as pointers live in ptrData, next to the
// 32-bit rawData they share on the N64.
static void compare_object(int slot, uint32_t n64_base, const struct Object *native) {
    char where[160];
    char behavior[96];
    describe(behavior, sizeof(behavior), native_target(native->behavior));
    snprintf(where, sizeof(where), "gObjectPool[%d] (%s)", slot, behavior);
    for (const struct leaf *l = LAYOUT_Object; l->path; ++l) {
        if (strcmp(l->path, "rawData.asU32") != 0) {
            for (uint32_t i = 0; i < l->count; ++i) {
                compare_value(where, l->path, i, l->kind, n64_base + l->n64_offset + i * l->n64_stride,
                              (const uint8_t *) native + l->native_offset + i * l->native_stride);
            }
            continue;
        }
        for (uint32_t i = 0; i < l->count; ++i) {
            const uint32_t n64_address = n64_base + l->n64_offset + 4 * i;
            if (native->ptrData.asVoidPtr[i] != NULL) {
                compare_value(where, "ptrData", i, LEAF_PTR, n64_address,
                              (const uint8_t *) &native->ptrData.asVoidPtr[i]);
            } else {
                // A slot used as two s16 (asS16[i][0..1]) keeps [0] in its
                // upper half on the big-endian N64 and in its lower half here.
                const uint32_t mine = native->rawData.asU32[i];
                if (n64_u32(n64_address) == (mine >> 16 | mine << 16)) {
                    continue;
                }
                compare_value(where, "rawData", i, LEAF_U32, n64_address, (const uint8_t *) &mine);
            }
        }
    }
}

typedef struct {
    const char *name;
    int kind;                  // a leaf kind for scalars, or -1 for structs
    const struct leaf *layout; // structs
    uint32_t n64_size, native_size, count;
    bool dereference;          // the global is a pointer to the struct
    bool overlay;              // lives in the Goddard/menu segment
} global;

static const uint32_t sKindSize[] = { 1, 1, 2, 2, 4, 4, 8, 8, 4, 8, 0 };
#define SCALAR(name, kind) { name, kind, NULL, 0, 0, 1, false, false }
#define SCALARS(name, kind, count) { name, kind, NULL, 0, 0, count, false, false }
#define MENU(name, kind, count) { name, kind, NULL, 0, 0, count, false, true }
#define STRUCTS(name, type, count) { name, -1, LAYOUT_##type, N64_SIZEOF_##type, sizeof(struct type), count, false, false }
#define POINTED(name, type) { name, -1, LAYOUT_##type, N64_SIZEOF_##type, sizeof(struct type), 1, true, false }

static const global sGlobals[] = {
    SCALAR("gGlobalTimer", LEAF_U32),
    SCALAR("gRandomSeed16", LEAF_U16),
    SCALAR("gCurrLevelNum", LEAF_S16),
    SCALAR("gCurrAreaIndex", LEAF_S16),
    SCALAR("gCurrCourseNum", LEAF_S16),
    SCALAR("gCurrActNum", LEAF_S16),
    SCALAR("gCurrSaveFileNum", LEAF_S16),
    SCALAR("gAreaUpdateCounter", LEAF_U16),
    SCALAR("gDialogID", LEAF_S16),
    SCALAR("gTimeStopState", LEAF_U32),
    SCALAR("sStatusFlags", LEAF_U16),
    SCALAR("gCameraMovementFlags", LEAF_U16),
    SCALAR("sSelectionFlags", LEAF_U16),
    SCALAR("gCutsceneTimer", LEAF_S16),
    SCALAR("sCutsceneShot", LEAF_S16),
    SCALAR("sAreaYaw", LEAF_S16),
    SCALAR("sAreaYawChange", LEAF_S16),
    SCALAR("sLakituDist", LEAF_S16),
    SCALAR("sLakituPitch", LEAF_S16),
    SCALAR("sCUpCameraPitch", LEAF_S16),
    SCALAR("sModeOffsetYaw", LEAF_S16),
    SCALAR("sPanDistance", LEAF_F32),
    SCALAR("sCannonYOffset", LEAF_F32),
    SCALAR("sYawSpeed", LEAF_S16),
    SCALAR("gCameraZoomDist", LEAF_F32),
    SCALAR("sFramesPaused", LEAF_U8),
    SCALAR("sHandheldShakeTimer", LEAF_F32),
    STRUCTS("sModeInfo", ModeTransitionInfo, 1),
    STRUCTS("sFOVState", CameraFOVStatus, 1),
    STRUCTS("sMarioGeometry", PlayerGeometry, 1),
    MENU("sLoadedActNum", LEAF_S8, 1),
    MENU("sObtainedStars", LEAF_U8, 1),
    MENU("sVisibleStars", LEAF_S8, 1),
    MENU("sInitSelectedActNum", LEAF_U8, 1),
    MENU("sSelectedActIndex", LEAF_S8, 1),
    MENU("sActSelectorMenuTimer", LEAF_S32, 1),
    MENU("sSelectedButtonID", LEAF_S8, 1),
    MENU("sCurrentMenuLevel", LEAF_S8, 1),
    MENU("sCursorPos", LEAF_F32, 2),
    MENU("sClickPos", LEAF_S16, 2),
    MENU("sSelectedFileIndex", LEAF_S8, 1),
    MENU("sFadeOutText", LEAF_S8, 1),
    MENU("sStatusMessageID", LEAF_S8, 1),
    MENU("sMainMenuTimer", LEAF_S16, 1),
    MENU("sSelectedFileNum", LEAF_S8, 1),
    SCALAR("gMarioObject", LEAF_PTR),
    SCALAR("gCurrentArea", LEAF_PTR),
    STRUCTS("gMarioStates", MarioState, 1),
    STRUCTS("gBodyStates", MarioBodyState, 2),
    STRUCTS("gLakituState", LakituState, 1),
    STRUCTS("gPlayerCameraState", PlayerCameraState, 2),
    STRUCTS("sCutsceneVars", CutsceneVariable, 10),
    STRUCTS("gControllers", Controller, 3),
    STRUCTS("gHudDisplay", HudDisplay, 1),
    STRUCTS("gSaveBuffer", SaveBuffer, 1),
    STRUCTS("sWarpDest", WarpDest, 1),
    STRUCTS("gAreaData", Area, 8),
    POINTED("gCamera", Camera),
};

// The Goddard/menu segment is loaded into the pool and later overwritten by
// level data. Its code, fingerprinted right after each load, tells whether
// it is still there.
extern unsigned gHostOverlayLoads;
static unsigned sOverlayLoadsSeen;
static uint32_t sOverlayFingerprint[16], sOverlayCode;
static bool sOverlayFingerprinted;

static bool overlay_resident(void) {
    if (!sOverlayFingerprinted) {
        return false;
    }
    for (int i = 0; i < 16; ++i) {
        if (n64_u32(sOverlayCode + 4 * i) != sOverlayFingerprint[i]) {
            return false;
        }
    }
    return true;
}

static void compare_all(void) {
    const bool resident = overlay_resident();
    for (size_t g = 0; g < sizeof(sGlobals) / sizeof(sGlobals[0]); ++g) {
        const global *gl = &sGlobals[g];
        if (gl->overlay && !resident) {
            continue;
        }
        const uint8_t *native = elf_symbol(gl->name, NULL);
        uint32_t n64 = n64_lookup(gl->name)->address;
        if (!native) {
            fprintf(stderr, "lockstep: no native symbol %s\n", gl->name);
            exit(1);
        }
        if (gl->layout == NULL) {
            for (uint32_t i = 0; i < gl->count; ++i) {
                compare_value(gl->name, "", i, gl->kind, n64 + i * sKindSize[gl->kind],
                              native + i * (gl->kind == LEAF_PTR ? sizeof(void *) : sKindSize[gl->kind]));
            }
            continue;
        }
        if (gl->dereference) {
            // gCamera dangles during level transitions: it keeps pointing at
            // the previous area's camera while the new level's data is loaded
            // over it. Only the current area's camera holds game state.
            if (strcmp(gl->name, "gCamera") == 0) {
                const uint32_t area = n64_u32(n64_lookup("gCurrentArea")->address);
                const struct leaf *camera = NULL;
                for (const struct leaf *l = LAYOUT_Area; l->path; ++l) {
                    if (strcmp(l->path, "camera") == 0) {
                        camera = l;
                    }
                }
                if (area == 0 || n64_u32(area + camera->n64_offset) != n64_u32(n64)) {
                    continue;
                }
            }
            n64 = n64_u32(n64);
            native = *(const uint8_t *const *) native;
            if ((n64 == 0) != (native == NULL)) {
                report("%s: %s -> %s", gl->name, n64 ? "set" : "NULL", native ? "set" : "NULL");
                continue;
            }
            if (!native) {
                continue;
            }
        }
        for (uint32_t i = 0; i < gl->count; ++i) {
            char where[96];
            snprintf(where, sizeof(where), gl->count > 1 ? "%s[%u]" : "%s", gl->name, i);
            compare_struct(where, gl->layout, n64 + i * gl->n64_size, native + i * gl->native_size);
        }
    }
    for (int slot = 0; slot < OBJECT_POOL_CAPACITY; ++slot) {
        compare_object(slot, sN64ObjectPool + slot * N64_SIZEOF_Object, &sNativeObjectPool[slot]);
    }
}

// --- Oracle hook ---------------------------------------------------------------

int lockstep_poll(const uint8_t *ram, uint32_t poll, uint32_t input) {
    sRam = ram;
    sPoll = poll;
    // The first poll is a controller read during boot, before the game loop.
    if (poll == 0) {
        load_table(&sKseg0, SM64_ORACLE_DIR "/symbols/" SM64_VERSION_NAME ".tsv");
        load_table(&sSegmented, SM64_ORACLE_DIR "/symbols/" SM64_VERSION_NAME "_segments.tsv");
        sN64ObjectPool = n64_lookup("gObjectPool")->address;
        sN64SegmentTable = n64_lookup("sSegmentTable")->address;
        sNativeObjectPool = elf_symbol("gObjectPool", NULL);
        sOverlayCode = n64_lookup("bhv_menu_button_init")->address;
        for (size_t i = 0; i < sizeof(sNamedSegments) / sizeof(sNamedSegments[0]); ++i) {
            sNamedSegmentSize[i] = segment_size(sNamedSegments[i]);
        }
        sm64_boot();
        return 0;
    }
    // A load in the frame just run: the segment is in the emulator's RAM now.
    if (gHostOverlayLoads != sOverlayLoadsSeen) {
        sOverlayLoadsSeen = gHostOverlayLoads;
        for (int i = 0; i < 16; ++i) {
            sOverlayFingerprint[i] = n64_u32(sOverlayCode + 4 * i);
        }
        sOverlayFingerprinted = true;
    }
    compare_all();
    if (sReported) {
        if (sReported > MAX_REPORTED) {
            printf("  ... %d more\n", sReported - MAX_REPORTED);
        }
        return 1;
    }
    if (poll % 1000 == 0) {
        fprintf(stderr, "lockstep: %u polls identical\n", poll);
    }
    sm64_step(input);
    return 0;
}
