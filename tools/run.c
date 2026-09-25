// Steps the native game with an oracle polls file (one u32 input per frame)
// and writes a trace in the oracle's format, the state laid out as on the N64
// (big endian, N64 offsets, pointers zero), for oracle/sm64trace.py diff.
//
//   sm64_run POLLS [--trace OUT] [--frames N]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include <ultra64.h>
#include "types.h"
#include "game/level_update.h"

#include "elf_symbols.h"
#include "sm64_physics.h"
#include "world.h"

// The one world this runs.
static sm64_world *sWorld;

extern char sm64_state_start[]; // platform/state.ld

// --- N64 layout ------------------------------------------------------------------

typedef struct {
    unsigned char *data;
    size_t size;
} buffer;

static void be16(unsigned char *p, uint16_t v) {
    p[0] = v >> 8;
    p[1] = v;
}
static void be32(unsigned char *p, uint32_t v) {
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
}
static void bef(unsigned char *p, float v) {
    uint32_t bits;
    memcpy(&bits, &v, 4);
    be32(p, bits);
}

// struct MarioState on the N64 (0xC8 bytes); pointer members stay zero.
static void mario_state_n64(unsigned char *p, const struct MarioState *m) {
    memset(p, 0, 0xC8);
    be16(p + 0x00, m->unk00);
    be16(p + 0x02, m->input);
    be32(p + 0x04, m->flags);
    be32(p + 0x08, m->particleFlags);
    be32(p + 0x0C, m->action);
    be32(p + 0x10, m->prevAction);
    be32(p + 0x14, m->terrainSoundAddend);
    be16(p + 0x18, m->actionState);
    be16(p + 0x1A, m->actionTimer);
    be32(p + 0x1C, m->actionArg);
    bef(p + 0x20, m->intendedMag);
    be16(p + 0x24, m->intendedYaw);
    be16(p + 0x26, m->invincTimer);
    p[0x28] = m->framesSinceA;
    p[0x29] = m->framesSinceB;
    p[0x2A] = m->wallKickTimer;
    p[0x2B] = m->doubleJumpTimer;
    for (int i = 0; i < 3; ++i) {
        be16(p + 0x2C + 2 * i, m->faceAngle[i]);
        be16(p + 0x32 + 2 * i, m->angleVel[i]);
        bef(p + 0x3C + 4 * i, m->pos[i]);
        bef(p + 0x48 + 4 * i, m->vel[i]);
    }
    be16(p + 0x38, m->slideYaw);
    be16(p + 0x3A, m->twirlYaw);
    bef(p + 0x54, m->forwardVel);
    bef(p + 0x58, m->slideVelX);
    bef(p + 0x5C, m->slideVelZ);
    bef(p + 0x6C, m->ceilHeight);
    bef(p + 0x70, m->floorHeight);
    be16(p + 0x74, m->floorAngle);
    be16(p + 0x76, m->waterLevel);
    be32(p + 0xA4, m->collidedObjInteractTypes);
    be16(p + 0xA8, m->numCoins);
    be16(p + 0xAA, m->numStars);
    p[0xAC] = m->numKeys;
    p[0xAD] = m->numLives;
    be16(p + 0xAE, m->health);
    be16(p + 0xB0, m->unkB0);
    p[0xB2] = m->hurtCounter;
    p[0xB3] = m->healCounter;
    p[0xB4] = m->squishTimer;
    p[0xB5] = m->fadeWarpOpacity;
    be16(p + 0xB6, m->capTimer);
    be16(p + 0xB8, m->prevNumStarsForDialog);
    bef(p + 0xBC, m->peakHeight);
    bef(p + 0xC0, m->quicksandDepth);
    bef(p + 0xC4, m->gettingBlownGravity);
}

static void hud_display_n64(unsigned char *p, const struct HudDisplay *h) {
    be16(p + 0x0, h->lives);
    be16(p + 0x2, h->coins);
    be16(p + 0x4, h->stars);
    be16(p + 0x6, h->wedges);
    be16(p + 0x8, h->keys);
    be16(p + 0xA, h->flags);
    be16(p + 0xC, h->timer);
}

// --- Fields -------------------------------------------------------------------

typedef enum { RAW16, RAW32, MARIO, HUD } kind;
typedef struct {
    const char *name;
    kind kind;
    unsigned size;
    const void *address;
} field;

static field fields[] = {
    { "gGlobalTimer", RAW32, 4 },      { "gRandomSeed16", RAW16, 2 },     { "gCurrLevelNum", RAW16, 2 },
    { "gCurrAreaIndex", RAW16, 2 },    { "gCurrCourseNum", RAW16, 2 },    { "gCurrActNum", RAW16, 2 },
    { "gAreaUpdateCounter", RAW16, 2 }, { "gMarioStates", MARIO, 0xC8 },   { "gHudDisplay", HUD, 14 },
    { "gDialogID", RAW16, 2 },
};
#define FIELD_COUNT (sizeof(fields) / sizeof(fields[0]))

static void write_header(FILE *out) {
    fwrite("SM64ORC1", 1, 8, out);
    uint32_t count = FIELD_COUNT;
    fwrite(&count, 4, 1, out);
    for (unsigned f = 0; f < FIELD_COUNT; ++f) {
        uint16_t length = (uint16_t) strlen(fields[f].name);
        uint32_t address = 0, size = fields[f].size, stride = 0;
        fwrite(&length, 2, 1, out);
        fwrite(fields[f].name, 1, length, out);
        fwrite(&address, 4, 1, out);
        fwrite(&size, 4, 1, out);
        fwrite(&stride, 4, 1, out);
    }
}

static void write_record(FILE *out, uint32_t frame, uint32_t input) {
    uint32_t zero = 0;
    fwrite(&frame, 4, 1, out);
    fwrite(&zero, 4, 1, out);
    fwrite(&input, 4, 1, out);
    for (unsigned f = 0; f < FIELD_COUNT; ++f) {
        unsigned char bytes[0x100];
        const field *fd = &fields[f];
        // The symbol's address in the section, moved to the world.
        const void *address = (const char *) fd->address + gHostWorldOffset;
        switch (fd->kind) {
            case RAW16: be16(bytes, *(const uint16_t *) address); break;
            case RAW32: be32(bytes, *(const uint32_t *) address); break;
            case MARIO: mario_state_n64(bytes, address); break;
            case HUD: hud_display_n64(bytes, address); break;
        }
        fwrite(bytes, 1, fd->size, out);
    }
}

// A hash of the world's state that does not depend on where its memory is:
// its trace record (the oracle's fields).
static uint64_t hash_state(void *buffer) {
    (void) buffer;
    char *record = NULL;
    size_t size = 0;
    FILE *out = open_memstream(&record, &size);
    sm64_world_enter(sWorld);
    write_record(out, 0, 0);
    fclose(out);
    uint64_t hash = 0xcbf29ce484222325u;
    for (size_t i = 0; i < size; ++i) {
        hash = (hash ^ (unsigned char) record[i]) * 0x100000001b3u;
    }
    free(record);
    return hash;
}

// --check-state FRAME: save the state after FRAME, run to the end, load it
// and run again: the states along the way have to be the same.
static int check_state(const uint32_t *inputs, uint32_t count, uint32_t at) {
    enum { EVERY = 500 };
    void *saved = malloc(sm64_state_size()), *scratch = malloc(sm64_state_size());
    uint64_t hashes[1024];
    unsigned checks = 0;
    for (uint32_t frame = 0; frame < count; ++frame) {
        sm64_step(sWorld, inputs[frame]);
        if (frame + 1 == at) {
            sm64_save_state(sWorld, saved);
        } else if (frame + 1 > at && (frame + 1 - at) % EVERY == 0 && checks < 1024) {
            hashes[checks++] = hash_state(scratch);
        }
    }
    // Into another world: the state's addresses move to its memory.
    sm64_world *other = sm64_world_create();
    sm64_world_destroy(sWorld);
    sWorld = other;
    if (!sm64_load_state(sWorld, saved)) {
        printf("not a saved state\n");
        return 1;
    }
    unsigned check = 0;
    for (uint32_t frame = at; frame < count; ++frame) {
        sm64_step(sWorld, inputs[frame]);
        if ((frame + 1 - at) % EVERY == 0 && check < checks) {
            if (hash_state(scratch) != hashes[check++]) {
                printf("state differs after frame %u\n", frame + 1);
                return 1;
            }
        }
    }
    printf("%zu byte state: %u checks from frame %u identical\n", sm64_state_size(), check, at);
    return 0;
}

// --threads N: N threads each run their own world through all the inputs at
// the same time. Each frame's trace record (the oracle's fields) and, at the
// end, every variable but the memory pool (whose freed memory keeps parts of
// each world's own addresses) have to be the same in all of them.
typedef struct {
    const uint32_t *inputs;
    uint32_t count;
    uint64_t trace_hash;
    unsigned char *final; // the last state, addresses in the world relative to it
} thread_run;

extern char sm64_state_start[]; // platform/state.ld

static int run_world(void *arg) {
    thread_run *run = arg;
    sm64_world *world = sm64_world_create();
    sm64_world_enter(world);
    char *record = NULL;
    size_t record_size = 0;
    uint64_t hash = 0xcbf29ce484222325u;
    for (uint32_t frame = 0; frame < run->count; ++frame) {
        FILE *out = open_memstream(&record, &record_size);
        write_record(out, frame + 1, run->inputs[frame]);
        fclose(out);
        for (size_t i = 0; i < record_size; ++i) {
            hash = (hash ^ (unsigned char) record[i]) * 0x100000001b3u;
        }
        free(record);
        sm64_step(world, run->inputs[frame]);
    }
    run->trace_hash = hash;
    run->final = malloc(sm64_state_size());
    sm64_save_state(world, run->final);
    const uintptr_t base = (uintptr_t) sm64_state_start + gHostWorldOffset;
    for (size_t i = 0; i + 8 <= sm64_state_size(); i += 4) {
        uint64_t w;
        memcpy(&w, run->final + i, 8);
        if (w >= base && w < base + sm64_state_size()) {
            w -= base;
            memcpy(run->final + i, &w, 8);
            i += 4;
        }
    }
    sm64_world_destroy(world);
    return 0;
}

static int check_threads(const uint32_t *inputs, uint32_t count, int threads) {
    thrd_t ids[64];
    thread_run runs[64];
    for (int t = 0; t < threads; ++t) {
        runs[t] = (thread_run) { inputs, count, 0, NULL };
        thrd_create(&ids[t], run_world, &runs[t]);
    }
    for (int t = 0; t < threads; ++t) {
        thrd_join(ids[t], NULL);
    }
    size_t pool_offset = 0, pool_size = 0;
    const char *pool = elf_symbol("sPoolMemory", &pool_size);
    pool_offset = pool - sm64_state_start;
    int differ = 0;
    for (int t = 1; t < threads; ++t) {
        if (runs[t].trace_hash != runs[0].trace_hash) {
            printf("thread %d: the trace differs\n", t);
            differ = 1;
        }
        for (size_t i = 0; i < sm64_state_size(); ++i) {
            if (i == pool_offset) {
                i += pool_size - 1;
                continue;
            }
            if (runs[t].final[i] != runs[0].final[i]) {
                size_t offset = 0, size = 0;
                const char *name = elf_symbol_containing(sm64_state_start + i, &offset, &size);
                printf("thread %d: %s+0x%zx differs\n", t, name ? name : "?", offset);
                differ = 1;
                break;
            }
        }
    }
    printf("%d threads, %u frames each: %s\n", threads, count, differ ? "different" : "identical");
    return differ;
}

// A saved state (platform/world.c): a 16-byte header, the memory, the map.
#define SAVED_HEADER 16

static size_t memory_size(void) {
    extern char sm64_state_end[];
    return sm64_state_end - sm64_state_start;
}

// --pointers EVERY: two worlds stepped alike at different addresses. Every
// EVERY frames, the words where they differ by exactly the distance between
// them are the state's addresses of itself; those the pointer map misses are
// printed as the variable (or pool offset) they are in.
static int find_pointers(const uint32_t *inputs, uint32_t count, uint32_t every) {
    sm64_world *a = sm64_world_create(), *b = sm64_world_create();
    const size_t size = memory_size();
    unsigned char *sa = malloc(sm64_state_size()), *sb = malloc(sm64_state_size());
    unsigned char *ma = sa + SAVED_HEADER, *mb = sb + SAVED_HEADER;
    const uint32_t *map = (const uint32_t *) (ma + size);
    sm64_world_enter(a);
    const uintptr_t base_a = (uintptr_t) sm64_state_start + gHostWorldOffset;
    sm64_world_enter(b);
    const uintptr_t base_b = (uintptr_t) sm64_state_start + gHostWorldOffset;
    unsigned long missing = 0, found = 0, dead = 0;
    for (uint32_t frame = 0; frame < count; ++frame) {
        sm64_step(a, inputs[frame]);
        sm64_step(b, inputs[frame]);
        if ((frame + 1) % every != 0) {
            continue;
        }
        sm64_save_state(a, sa);
        sm64_save_state(b, sb);
        const char *last = NULL;
        for (size_t i = 0; i + 8 <= size; i += 4) {
            uint64_t wa, wb;
            memcpy(&wa, ma + i, 8);
            memcpy(&wb, mb + i, 8);
            if (wa == wb || wa < base_a || wa >= base_a + size || wa - base_a != wb - base_b) {
                continue;
            }
            ++found;
            if (!(map[i / 4 / 32] >> (i / 4 % 32) & 1)) {
                ++missing;
                size_t offset = 0, sym_size = 0;
                const char *name = elf_symbol_containing(sm64_state_start + i, &offset, &sym_size);
                size_t t_offset = 0, t_size = 0;
                const char *target = elf_symbol_containing(sm64_state_start + (wa - base_a), &t_offset, &t_size);
                // Memory handed out at run time: the code that allocated it.
                size_t fo = 0, fs = 0;
                void *caller = sm64_world_allocation_at(a, i);
                const char *site = caller ? elf_symbol_containing(caller, &fo, &fs) : "";
                if (!site) {
                    site = "?";
                }
                void *target_caller = sm64_world_allocation_at(a, wa - base_a);
                const char *target_site = target_caller ? elf_symbol_containing(target_caller, &fo, &fs) : "";
                // Memory the game hands out (the pools and heaps): where nothing
                // allocated holds, nothing reads what is there.
                const bool heap = name && (strcmp(name, "sPoolMemory") == 0 || strcmp(name, "gZBuffer") == 0
                                           || strcmp(name, "gFramebuffers") == 0 || strcmp(name, "gAudioHeap") == 0);
                if (*site == '\0' && heap) {
                    // Memory no allocation holds: nothing reads what is there.
                    ++dead;
                    --missing;
                    i += 4;
                    continue;
                }
                if (name != last || heap) {
                    printf("%u\t%zx\t%s+0x%zx\t%s\t-> %s+0x%zx\t%s\n", frame + 1, i, name ? name : "?", offset, site,
                           target ? target : "?", t_offset, target_site ? target_site : "?");
                }
                last = name;
            }
            i += 4;
        }
    }
    printf("%lu addresses found, %lu not in the map, %lu more in memory no allocation holds\n", found, missing, dead);
    return missing != 0;
}

// --move EVERY: every EVERY frames the world is copied to new memory and the
// old one overwritten and freed: an address the copy did not move faults or
// changes the run (compare --trace with a run without --move).
static sm64_world *move_world(sm64_world *world) {
    sm64_world *copy = sm64_world_clone(world);
    sm64_world_enter(world);
    memset(sm64_state_start + gHostWorldOffset, 0xAB, memory_size());
    sm64_world_destroy(world);
    sm64_world_enter(copy);
    return copy;
}

// --draw: every frame's display list, walked; the textures it names that are
// not from the ROM (sm64_texture) are counted: the game's own, such as the JP
// dialog font's glyphs unpacked into the display list pool.
struct drawn {
    unsigned long lists, commands, textures, unresolved;
};

static void walk_list(const Gfx *list, struct drawn *drawn, int depth) {
    for (unsigned long n = 0; depth < 32 && n < 1u << 20; ++n, ++list) {
        ++drawn->commands;
        const uintptr_t w0 = list->words.w0, w1 = list->words.w1;
        switch ((uint8_t) (w0 >> 24)) {
            case (uint8_t) G_DL:
                if (((w0 >> 16) & 0xff) == G_DL_NOPUSH) {
                    list = (const Gfx *) w1 - 1;
                } else {
                    walk_list((const Gfx *) w1, drawn, depth + 1);
                }
                break;
            case (uint8_t) G_ENDDL:
                return;
            case (uint8_t) G_SETTIMG:
                ++drawn->textures;
                if (!sm64_texture((const void *) w1)) {
                    ++drawn->unresolved;
                }
                break;
        }
    }
}

int main(int argc, char **argv) {
    const char *polls_path = NULL, *trace_path = NULL;
    bool audio = false, draw = false;
    const char *rom_path = NULL;
    long limit = -1, check_at = -1, threads = 0, pointers = 0, move = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--trace") == 0 && i + 1 < argc) {
            trace_path = argv[++i];
        } else if (strcmp(argv[i], "--rom") == 0 && i + 1 < argc) {
            rom_path = argv[++i];
        } else if (strcmp(argv[i], "--audio") == 0) {
            audio = true;
        } else if (strcmp(argv[i], "--draw") == 0) {
            draw = true;
        } else if (strcmp(argv[i], "--move") == 0 && i + 1 < argc) {
            move = atol(argv[++i]);
        } else if (strcmp(argv[i], "--pointers") == 0 && i + 1 < argc) {
            pointers = atol(argv[++i]);
        } else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            threads = atol(argv[++i]);
        } else if (strcmp(argv[i], "--check-state") == 0 && i + 1 < argc) {
            check_at = atol(argv[++i]);
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            limit = atol(argv[++i]);
        } else {
            polls_path = argv[i];
        }
    }
    if (!polls_path) {
        fprintf(stderr, "usage: sm64_run POLLS [--trace OUT] [--frames N] [--rom ROM] [--audio] [--draw] [--check-state FRAME] [--threads N]\n");
        return 1;
    }
    FILE *polls = fopen(polls_path, "rb");
    if (!polls) {
        perror(polls_path);
        return 1;
    }
    FILE *trace = NULL;
    if (trace_path || threads > 0 || check_at >= 0) {
        for (unsigned f = 0; f < FIELD_COUNT; ++f) {
            if (!(fields[f].address = elf_symbol(fields[f].name, NULL))) {
                fprintf(stderr, "sm64_run: no symbol %s\n", fields[f].name);
                return 1;
            }
        }
    }
    if (trace_path) {
        if (!(trace = fopen(trace_path, "wb"))) {
            perror(trace_path);
            return 1;
        }
        write_header(trace);
    }
    // The oracle records at each poll, before the frame that reads it runs:
    // boot first, then record before every step. Its first poll is a
    // controller read during boot, before the game loop; game frame N reads
    // poll N + 1, and records carry the poll number to line up.
    if (rom_path) {
        FILE *rom = fopen(rom_path, "rb");
        if (!rom) {
            perror(rom_path);
            return 1;
        }
        fseek(rom, 0, SEEK_END);
        const long size = ftell(rom);
        fseek(rom, 0, SEEK_SET);
        void *bytes = malloc(size);
        if (fread(bytes, 1, size, rom) != (size_t) size || !sm64_load_rom(bytes, size)) {
            fprintf(stderr, "sm64_run: %s is not the ROM this library is built for\n", rom_path);
            return 1;
        }
        free(bytes);
        fclose(rom);
    }
    sm64_set_audio(audio);
    sm64_set_draw(draw);
    sWorld = sm64_world_create();
    sm64_world_enter(sWorld);
    uint32_t input;
    if (fread(&input, 4, 1, polls) != 1) {
        fprintf(stderr, "sm64_run: %s is empty\n", polls_path);
        return 1;
    }
    if (check_at >= 0 || threads > 0 || pointers > 0) {
        static uint32_t inputs[1 << 20];
        uint32_t count = 0;
        while (count < (1 << 20) && fread(&inputs[count], 4, 1, polls) == 1) {
            ++count;
        }
        if (pointers > 0) {
            return find_pointers(inputs, count, (uint32_t) pointers);
        }
        return threads > 0 ? check_threads(inputs, count, threads > 64 ? 64 : threads)
                           : check_state(inputs, count, (uint32_t) check_at);
    }
    struct drawn drawn = { 0 };
    uint32_t frame = 0;
    while ((limit < 0 || frame < limit) && fread(&input, 4, 1, polls) == 1) {
        if (trace) {
            write_record(trace, frame + 1, input);
        }
        if (draw) {
            const Gfx *list = sm64_step_draw(sWorld, input);
            if (list) {
                ++drawn.lists;
                walk_list(list, &drawn, 0);
            }
        } else {
            sm64_step(sWorld, input);
        }
        ++frame;
        if (move > 0 && frame % move == 0) {
            sWorld = move_world(sWorld);
        }
    }
    fclose(polls);
    if (trace) {
        fclose(trace);
    }
    printf("%u frames\n", frame);
    if (draw) {
        printf("%lu display lists, %lu commands, %lu textures, %lu not from the ROM\n", drawn.lists, drawn.commands,
               drawn.textures, drawn.unresolved);
    }
    return 0;
}
