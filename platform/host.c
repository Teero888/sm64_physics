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
#ifdef VERSION_SH
#include "game/rumble_init.h"
#endif
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

// How many times the Goddard/menu segment was loaded: the lockstep comparator
// only reads the menus' variables in the emulator while the segment is there.
unsigned gHostOverlayLoads;

// platform/world.c
void host_restore_overlay(void);
void host_restore_level_data(void);
extern __thread bool gHostRunAudio __attribute__((tls_model("initial-exec")));

// FIXED_LOAD (docs/changes.md 6): the Goddard/menu segment is read from ROM again.
void host_reload_overlay(void) {
    WORLD(gHostOverlayLoads)++;
    host_restore_overlay();
}

// LOAD_MIO0 of segment 7 (docs/changes.md 8).
void host_reload_level_data(void) {
    host_restore_level_data();
}

static struct LevelCommand *sLevelAddress;
// The N64 pool runs from the end of the framebuffers to the end of RDRAM;
// pointers are twice as wide here.
static u8 sPoolMemory[DOUBLE_SIZE_ON_64_BIT(SEG_POOL_SIZE)] __attribute__((aligned(16)));

// --- The ROM -----------------------------------------------------------------
// The library carries the game's code and data, but not what the decomp takes
// from the ROM: the demo inputs the title screen plays (and, for drawing and
// sound, textures and sound banks).

void host_load_demo_inputs(const unsigned char *rom); // game/gen/<version>/assets/demo_data.c

// The demo inputs, from a normalized ROM (platform/rom.c), into the current
// world: sm64_load_rom calls it on the initial values.
void host_load_rom(const unsigned char *rom) {
    host_load_demo_inputs(rom);
}

// Power-on: what the console does before the first game frame (platform/world.c).
void host_boot(void) {
    // thread3_main
    setup_mesg_queues();
    main_pool_init(WORLD(sPoolMemory), WORLD(sPoolMemory) + sizeof(WORLD(sPoolMemory)));
    WORLD(gEffectsMemoryPool) = mem_pool_init(0x4000, MEMORY_POOL_LEFT);

    // thread4_sound, up to its loop
    audio_init();
    sound_init();

    // thread5_game_loop, up to its loop
    setup_game_memory();
#ifdef VERSION_SH
    init_rumble_pak_scheduler_queue();
#endif
    init_controllers();
#ifdef VERSION_SH
    // thread6 starts at once (its priority is above the game thread's) and
    // then waits for vertical interrupts.
    create_thread_6();
    rumble_thread_start();
#endif
    save_file_load_all();
    WORLD(sLevelAddress) = segmented_to_virtual(level_script_entry);
    play_music(SEQ_PLAYER_SFX, SEQUENCE_ARGS(0, SEQ_SOUND_PLAYER), 0);
    set_sound_mode(save_file_get_sound_mode());
    render_init();
}



// mupen64plus's BUTTONS value: the controller's two button bytes in bits 0-15
// (first byte low), stick X in bits 16-23, stick Y in 24-31.
static void latch_input(uint32_t value) {
    WORLD(gHostPad).button = (u16) ((value & 0xff) << 8 | (value >> 8 & 0xff));
    WORLD(gHostPad).stick_x = (s8) (value >> 16);
    WORLD(gHostPad).stick_y = (s8) (value >> 24);
    WORLD(gHostPad).errnum = 0;
}

extern volatile s32 gAudioFrameCount;

// The sound thread's work (thread4_sound) only produces sound: the game reads
// nothing back from it. It mixes the sequences and moves the game's sound
// requests into sound banks that only the sound thread uses. A simulation
// skips it; sm64_set_audio turns it on for sound output.
// One iteration of thread4_sound's loop. The game waits for one in
// sound_reset (docs/changes.md 2): without sound, the frame just counts.
void host_run_audio_frame(void) {
    if (gHostRunAudio) {
        create_next_audio_frame_task();
    } else {
        WORLD(gAudioFrameCount)++;
    }
}

// One game frame of the current world (platform/world.c).
void host_step(uint32_t input) {

    latch_input(input);
    // One iteration of thread5_game_loop.
    if (WORLD(gResetTimer) != 0) {
        draw_reset_bars();
        return;
    }
#ifdef VERSION_SH
    if (WORLD(gControllerBits)) {
        block_until_rumble_pak_free();
    }
#endif
    audio_game_loop_tick();
    select_gfx_pool();
    read_controller_inputs();
    // The level script runs from thread5_game_loop's frame on the N64.
    gN64StackPointer = N64_GAME_LOOP_SP;
    WORLD(sLevelAddress) = level_script_execute(WORLD(sLevelAddress));
    display_and_vsync();
#ifdef VERSION_SH
    // The rumble thread runs once per vertical interrupt, two per game frame.
    // (A lag frame would have more; the host has no timing to know of them.)
    for (int vi = 0; vi < 2; ++vi) {
        WORLD(gNumVblanks)++;
        rumble_thread_vi();
    }
#endif
    // thread4_sound runs once per vertical interrupt, two per game frame.
    if (gHostRunAudio) {
        for (int vi = 0; vi < 2; ++vi) {
            host_run_audio_frame();
        }
    }
}

// Library: its variables' addresses (tools/state/types.py).
#include "pointers/platform/host.c.inc.c"
