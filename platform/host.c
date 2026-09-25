// Runs the game one frame at a time: what thread3_main, thread4_sound and
// thread5_game_loop do on the N64, without threads. Nothing here changes game
// logic; it replaces the console's memory map and scheduling.
#include <ultra64.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "sm64_physics.h"
#include "n64stack.h"
#include "n64_frames.h"

#include "audio/external.h"
#include "buffers/framebuffers.h"
#include "buffers/gfx_output_buffer.h"
#include "buffers/zbuffer.h"
#include "engine/level_script.h"
#include "game/game_init.h"
#include "game/level_update.h"
#include "game/main.h"
#include "game/memory.h"
#include "game/save_file.h"
#include "game/sound_init.h"
#include "seq_ids.h"
#include "segments.h"

// Game functions without a prototype in the decomp's headers.
struct SPTask *create_next_audio_frame_task(void);
void setup_mesg_queues(void);
void init_controllers(void);
void read_controller_inputs(void);
void draw_reset_bars(void);

// The input osContGetReadData hands the game (platform/ultra.c).
OSContPad gHostPad;

// Variables the N64 resets by loading their segment from ROM again, linked
// into their own sections (sm64_section_group in CMakeLists.txt). The linker
// provides the sections' bounds.
#define SECTION_BOUNDS(prefix)                                                                         \
    extern char __start_##prefix##_data[], __stop_##prefix##_data[];                                   \
    extern char __start_##prefix##_datarel[], __stop_##prefix##_datarel[];                             \
    extern char __start_##prefix##_datarel2[], __stop_##prefix##_datarel2[];                           \
    extern char __start_##prefix##_bss[], __stop_##prefix##_bss[];
SECTION_BOUNDS(sm64ovl)
SECTION_BOUNDS(sm64lvl)
// Everything else, this file included.
SECTION_BOUNDS(sm64st)

typedef struct {
    char *start, *stop, *initial;
} saved_section;

typedef struct {
    saved_section data[3];
    char *bss_start, *bss_stop;
} section_group;

#define SECTION_GROUP(prefix)                                                                          \
    { { { __start_##prefix##_data, __stop_##prefix##_data, NULL },                                     \
        { __start_##prefix##_datarel, __stop_##prefix##_datarel, NULL },                               \
        { __start_##prefix##_datarel2, __stop_##prefix##_datarel2, NULL } },                           \
      __start_##prefix##_bss, __stop_##prefix##_bss }

// src/menu and src/goddard, and every level's data.
static section_group sOverlay = SECTION_GROUP(sm64ovl);
static section_group sLevelData = SECTION_GROUP(sm64lvl);

static void save_initial_values(section_group *group) {
    for (int i = 0; i < 3; ++i) {
        saved_section *section = &group->data[i];
        const size_t size = section->stop - section->start;
        section->initial = malloc(size ? size : 1);
        memcpy(section->initial, section->start, size);
    }
}

static void restore_initial_values(section_group *group) {
    for (int i = 0; i < 3; ++i) {
        saved_section *section = &group->data[i];
        memcpy(section->start, section->initial, section->stop - section->start);
    }
    memset(group->bss_start, 0, group->bss_stop - group->bss_start);
}

// How many times the Goddard/menu segment was loaded: the lockstep comparator
// only reads the menus' variables in the emulator while the segment is there.
unsigned gHostOverlayLoads;

// FIXED_LOAD (docs/changes.md 6): the Goddard/menu segment is read from ROM again.
void host_reload_overlay(void) {
    gHostOverlayLoads++;
    restore_initial_values(&sOverlay);
}

// LOAD_MIO0 of segment 7 (docs/changes.md 8): the level's data is decompressed
// from ROM again. The other levels' data is not in use, so restoring all of it
// is the same.
void host_reload_level_data(void) {
    restore_initial_values(&sLevelData);
}

static struct LevelCommand *sLevelAddress;
static bool sBooted;
// The N64 pool runs from the end of the framebuffers to the end of RDRAM;
// pointers are twice as wide here.
static u8 sPoolMemory[DOUBLE_SIZE_ON_64_BIT(SEG_POOL_SIZE)] __attribute__((aligned(16)));

// --- The ROM -----------------------------------------------------------------
// The library carries the game's code and data, but not what the decomp takes
// from the ROM: the demo inputs the title screen plays (and, for drawing and
// sound, textures and sound banks).

void host_load_demo_inputs(const unsigned char *rom); // game/gen/<version>/assets/demo_data.c

// Game code and checksums in the ROM header, big-endian (.z64) byte order.
#ifdef VERSION_JP
static const char sRomCode[4] = "NSMJ";
static const u32 sRomCrc[2] = { 0x4eaa3d0e, 0x74757c24 };
#else
static const char sRomCode[4] = "NSME";
static const u32 sRomCrc[2] = { 0x635a2bff, 0x8b022326 };
#endif


static u32 read_be32(const unsigned char *p) {
    return (u32) p[0] << 24 | (u32) p[1] << 16 | (u32) p[2] << 8 | p[3];
}

bool sm64_load_rom(const void *data, size_t size) {
    // .z64 is big-endian, .v64 swaps each 16-bit word, .n64 each 32-bit one.
    const unsigned char *in = data;
    if (size < 0x800000 || size % 4 != 0) {
        return false;
    }
    int swap;
    if (in[0] == 0x80 && in[1] == 0x37) {
        swap = 0;
    } else if (in[0] == 0x37 && in[1] == 0x80) {
        swap = 1;
    } else if (in[0] == 0x40 && in[3] == 0x80) {
        swap = 3;
    } else {
        return false;
    }
    unsigned char *rom = malloc(size);
    if (!rom) {
        return false;
    }
    for (size_t i = 0; i < size; ++i) {
        rom[i] = in[swap == 1 ? i ^ 1 : swap == 3 ? i ^ 3 : i];
    }
    const bool match = memcmp(rom + 0x3b, sRomCode, 4) == 0 && read_be32(rom + 0x10) == sRomCrc[0]
                       && read_be32(rom + 0x14) == sRomCrc[1];
    if (match) {
        host_load_demo_inputs(rom);
    }
    free(rom);
    return match;
}

void sm64_boot(void) {
    save_initial_values(&sOverlay);
    save_initial_values(&sLevelData);
    // thread3_main
    setup_mesg_queues();
    main_pool_init(sPoolMemory, sPoolMemory + sizeof(sPoolMemory));
    gEffectsMemoryPool = mem_pool_init(0x4000, MEMORY_POOL_LEFT);

    // thread4_sound, up to its loop
    audio_init();
    sound_init();

    // thread5_game_loop, up to its loop
    setup_game_memory();
    init_controllers();
    save_file_load_all();
    sLevelAddress = segmented_to_virtual(level_script_entry);
    play_music(SEQ_PLAYER_SFX, SEQUENCE_ARGS(0, SEQ_SOUND_PLAYER), 0);
    set_sound_mode(save_file_get_sound_mode());
    render_init();
    sBooted = true;
}

// mupen64plus's BUTTONS value: the controller's two button bytes in bits 0-15
// (first byte low), stick X in bits 16-23, stick Y in 24-31.
static void latch_input(uint32_t value) {
    gHostPad.button = (u16) ((value & 0xff) << 8 | (value >> 8 & 0xff));
    gHostPad.stick_x = (s8) (value >> 16);
    gHostPad.stick_y = (s8) (value >> 24);
    gHostPad.errnum = 0;
}

extern volatile s32 gAudioFrameCount;

// The sound thread's work (thread4_sound) only produces sound: the game reads
// nothing back from it. It mixes the sequences and moves the game's sound
// requests into sound banks that only the sound thread uses. A simulation
// skips it; sm64_set_audio turns it on for sound output.
static bool sRunAudio;

void sm64_set_audio(bool enabled) {
    sRunAudio = enabled;
}

// platform/draw.h
int gHostDraw;

void sm64_set_draw(bool enabled) {
    gHostDraw = enabled;
}

// One iteration of thread4_sound's loop. The game waits for one in
// sound_reset (docs/changes.md 2): without sound, the frame just counts.
void host_run_audio_frame(void) {
    if (sRunAudio) {
        create_next_audio_frame_task();
    } else {
        gAudioFrameCount++;
    }
}

// --- State -------------------------------------------------------------------
// The game's state is every variable of the game and of the host's stand-ins
// for the console: the writable sections of the three section groups, less
// the buffers that only hold the console's output.

typedef struct {
    char *start, *stop;
} state_range;

static state_range sStateRanges[32];
static int sStateRangeCount;
static size_t sStateSize;

static void add_state_range(char *start, char *stop) {
    static const struct {
        void *start;
        size_t size;
    } output[] = {
        { gFramebuffers, sizeof(gFramebuffers) },
        { gZBuffer, sizeof(gZBuffer) },
        { gGfxSPTaskOutputBuffer, sizeof(gGfxSPTaskOutputBuffer) },
    };
    for (size_t i = 0; i < sizeof(output) / sizeof(output[0]); ++i) {
        char *skip = output[i].start, *skip_end = skip + output[i].size;
        if (skip < stop && skip_end > start) {
            add_state_range(start, skip > start ? skip : start);
            add_state_range(skip_end < stop ? skip_end : stop, stop);
            return;
        }
    }
    if (start < stop) {
        sStateRanges[sStateRangeCount++] = (state_range) { start, stop };
        sStateSize += stop - start;
    }
}

static void find_state(void) {
    if (sStateRangeCount) {
        return;
    }
#define GROUP(prefix)                                                                                  \
    add_state_range(__start_##prefix##_data, __stop_##prefix##_data);                                  \
    add_state_range(__start_##prefix##_datarel, __stop_##prefix##_datarel);                            \
    add_state_range(__start_##prefix##_datarel2, __stop_##prefix##_datarel2);                          \
    add_state_range(__start_##prefix##_bss, __stop_##prefix##_bss);
    GROUP(sm64st)
    GROUP(sm64ovl)
    GROUP(sm64lvl)
#undef GROUP
}

size_t sm64_state_size(void) {
    find_state();
    return sStateSize;
}

void sm64_save_state(void *buffer) {
    if (!sBooted) {
        sm64_boot();
    }
    find_state();
    char *out = buffer;
    for (int i = 0; i < sStateRangeCount; ++i) {
        const size_t size = sStateRanges[i].stop - sStateRanges[i].start;
        memcpy(out, sStateRanges[i].start, size);
        out += size;
    }
}

void sm64_load_state(const void *buffer) {
    // The settings are not part of the state.
    const bool audio = sRunAudio;
    const int draw = gHostDraw;
    find_state();
    const char *in = buffer;
    for (int i = 0; i < sStateRangeCount; ++i) {
        const size_t size = sStateRanges[i].stop - sStateRanges[i].start;
        memcpy(sStateRanges[i].start, in, size);
        in += size;
    }
    sRunAudio = audio;
    gHostDraw = draw;
}

void sm64_step(uint32_t input) {
    if (!sBooted) {
        sm64_boot();
    }
    latch_input(input);
    // One iteration of thread5_game_loop.
    if (gResetTimer != 0) {
        draw_reset_bars();
        return;
    }
    audio_game_loop_tick();
    select_gfx_pool();
    read_controller_inputs();
    // The level script runs from thread5_game_loop's frame on the N64.
    gN64StackPointer = N64_GAME_LOOP_SP;
    sLevelAddress = level_script_execute(sLevelAddress);
    display_and_vsync();
    // thread4_sound runs once per vertical interrupt, two per game frame.
    if (sRunAudio) {
        for (int vi = 0; vi < 2; ++vi) {
            host_run_audio_frame();
        }
    }
}
