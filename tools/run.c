// Steps the native game with an oracle polls file (one u32 input per frame)
// and writes a trace in the oracle's format, the state laid out as on the N64
// (big endian, N64 offsets, pointers zero), for oracle/sm64trace.py diff.
//
//   sm64_run POLLS [--trace OUT] [--frames N]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ultra64.h>
#include "types.h"
#include "game/level_update.h"

#include "elf_symbols.h"
#include "sm64_physics.h"

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
        switch (fd->kind) {
            case RAW16: be16(bytes, *(const uint16_t *) fd->address); break;
            case RAW32: be32(bytes, *(const uint32_t *) fd->address); break;
            case MARIO: mario_state_n64(bytes, fd->address); break;
            case HUD: hud_display_n64(bytes, fd->address); break;
        }
        fwrite(bytes, 1, fd->size, out);
    }
}

int main(int argc, char **argv) {
    const char *polls_path = NULL, *trace_path = NULL;
    bool audio = false, draw = false;
    long limit = -1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--trace") == 0 && i + 1 < argc) {
            trace_path = argv[++i];
        } else if (strcmp(argv[i], "--audio") == 0) {
            audio = true;
        } else if (strcmp(argv[i], "--draw") == 0) {
            draw = true;
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            limit = atol(argv[++i]);
        } else {
            polls_path = argv[i];
        }
    }
    if (!polls_path) {
        fprintf(stderr, "usage: sm64_run POLLS [--trace OUT] [--frames N] [--audio] [--draw]\n");
        return 1;
    }
    FILE *polls = fopen(polls_path, "rb");
    if (!polls) {
        perror(polls_path);
        return 1;
    }
    FILE *trace = NULL;
    if (trace_path) {
        for (unsigned f = 0; f < FIELD_COUNT; ++f) {
            if (!(fields[f].address = elf_symbol(fields[f].name, NULL))) {
                fprintf(stderr, "sm64_run: no symbol %s\n", fields[f].name);
                return 1;
            }
        }
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
    sm64_set_audio(audio);
    sm64_set_draw(draw);
    sm64_boot();
    uint32_t input;
    if (fread(&input, 4, 1, polls) != 1) {
        fprintf(stderr, "sm64_run: %s is empty\n", polls_path);
        return 1;
    }
    uint32_t frame = 0;
    while ((limit < 0 || frame < limit) && fread(&input, 4, 1, polls) == 1) {
        if (trace) {
            write_record(trace, frame + 1, input);
        }
        sm64_step(input);
        ++frame;
    }
    fclose(polls);
    if (trace) {
        fclose(trace);
    }
    printf("%u frames\n", frame);
    return 0;
}
