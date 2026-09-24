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

// FIXED_LOAD (patches/0006): the Goddard/menu segment is read from ROM again.
void host_reload_overlay(void) {
    gHostOverlayLoads++;
    restore_initial_values(&sOverlay);
}

// LOAD_MIO0 of segment 7 (patches/0008): the level's data is decompressed
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

// One iteration of thread4_sound's loop. The game waits for one in
// sound_reset (patches/0002): without sound, the frame just counts.
void host_run_audio_frame(void) {
    if (sRunAudio) {
        create_next_audio_frame_task();
    } else {
        gAudioFrameCount++;
    }
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
