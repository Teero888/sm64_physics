// Runs the game one frame at a time: what thread3_main, thread4_sound and
// thread5_game_loop do on the N64, without threads. Nothing here changes game
// logic; it replaces the console's memory map and scheduling.
#include <ultra64.h>
#include <stdbool.h>
#include <string.h>

#include "sm64_physics.h"

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

static struct LevelCommand *sLevelAddress;
static bool sBooted;
// The N64 pool runs from the end of the framebuffers to the end of RDRAM;
// pointers are twice as wide here.
static u8 sPoolMemory[DOUBLE_SIZE_ON_64_BIT(SEG_POOL_SIZE)] __attribute__((aligned(16)));

void sm64_boot(void) {
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

// One iteration of thread4_sound's loop (patches/0002).
void host_run_audio_frame(void) {
    create_next_audio_frame_task();
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
    sLevelAddress = level_script_execute(sLevelAddress);
    display_and_vsync();
    // thread4_sound runs once per vertical interrupt, two per game frame. It
    // moves the game's sound requests into the sound banks the game reads.
    for (int vi = 0; vi < 2; ++vi) {
        host_run_audio_frame();
    }
}
