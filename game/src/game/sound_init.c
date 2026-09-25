#include <ultra64.h>

#include "area.h"
#include "audio/external.h"
#include "engine/graph_node.h"
#include "engine/math_util.h"
#include "level_table.h"
#include "level_update.h"
#include "main.h"
#include "paintings.h"
#include "print.h"
#include "profiler.h"
#include "save_file.h"
#include "seq_ids.h"
#include "sm64.h"
#include "sound_init.h"
#include "rumble_init.h"

#define MUSIC_NONE 0xFFFF

static Vec3f unused80339DC0;
static OSMesgQueue sSoundMesgQueue;
static OSMesg sSoundMesgBuf[1];
static struct VblankHandler sSoundVblankHandler;

// Only written to, never read.
static u8 sMusicVolume = 0;

static u8 sBgMusicDisabled = FALSE;
static u16 sCurrentMusic = MUSIC_NONE;
static u16 sCurrentShellMusic = MUSIC_NONE;
static u16 sCurrentCapMusic = MUSIC_NONE;
static u8 sPlayingInfiniteStairs = FALSE;
UNUSED static u8 unused8032C6D8[16] = { 0 };
static s16 sSoundMenuModeToSoundMode[] = { SOUND_MODE_STEREO, SOUND_MODE_MONO, SOUND_MODE_HEADSET };
// Only the 20th array element is used.
static u32 sMenuSoundsExtra[] = {
    SOUND_MOVING_TERRAIN_SLIDE + (0 << 16),
    SOUND_MOVING_TERRAIN_SLIDE + (1 << 16),
    SOUND_MOVING_TERRAIN_SLIDE + (2 << 16),
    SOUND_MOVING_TERRAIN_SLIDE + (3 << 16),
    SOUND_MOVING_TERRAIN_SLIDE + (4 << 16),
    SOUND_MOVING_TERRAIN_SLIDE + (5 << 16),
    SOUND_MOVING_TERRAIN_SLIDE + (6 << 16),
    SOUND_MOVING_LAVA_BURN,
    SOUND_MOVING_SLIDE_DOWN_TREE,
    SOUND_MOVING_SLIDE_DOWN_POLE,
    SOUND_MOVING_QUICKSAND_DEATH,
    SOUND_MOVING_TERRAIN_RIDING_SHELL,
    NO_SOUND,
    SOUND_ENV_BOAT_ROCKING1,
    SOUND_ENV_ELEVATOR3,
    SOUND_ENV_UNKNOWN2,
    SOUND_ENV_WATERFALL1,
    SOUND_ENV_WATERFALL2,
    SOUND_ENV_ELEVATOR1,
    SOUND_ENV_DRONING1,
    SOUND_ENV_DRONING2,
    SOUND_ENV_ELEVATOR2,
    SOUND_ENV_WIND1,
    SOUND_ENV_WATER,
    SOUND_AIR_BOWSER_SPIT_FIRE,
    SOUND_MOVING_AIM_CANNON,
    SOUND_AIR_LAKITU_FLY,
    SOUND_AIR_AMP_BUZZ,
    SOUND_AIR_CASTLE_OUTDOORS_AMBIENT,
    SOUND_OBJ2_BIRD_CHIRP1,
    SOUND_GENERAL2_BIRD_CHIRP2,
    SOUND_OBJ_BIRD_CHIRP3,
    SOUND_ENV_ELEVATOR2,
    SOUND_AIR_BLOW_WIND,
    SOUND_AIR_BLOW_FIRE,
    SOUND_ENV_ELEVATOR4,
};
static s8 sPaintingEjectSoundPlayed = FALSE;

void play_menu_sounds_extra(s32 a, void *b);

/**
 * Called from threads: thread5_game_loop
 */
void reset_volume(void) {
    WORLD(sMusicVolume) = 0;
}

/**
 * Called from threads: thread5_game_loop
 */
void lower_background_noise(s32 a) {
    switch (a) {
        case 1:
            set_audio_muted(TRUE);
            break;
        case 2:
            seq_player_lower_volume(SEQ_PLAYER_LEVEL, 60, 40);
            break;
    }
    WORLD(sMusicVolume) |= a;
}

/**
 * Called from threads: thread5_game_loop
 */
void raise_background_noise(s32 a) {
    switch (a) {
        case 1:
            set_audio_muted(FALSE);
            break;
        case 2:
            seq_player_unlower_volume(SEQ_PLAYER_LEVEL, 60);
            break;
    }
    WORLD(sMusicVolume) &= ~a;
}

/**
 * Called from threads: thread5_game_loop
 */
void disable_background_sound(void) {
    if (WORLD(sBgMusicDisabled) == FALSE) {
        WORLD(sBgMusicDisabled) = TRUE;
        sound_banks_disable(SEQ_PLAYER_SFX, SOUND_BANKS_BACKGROUND);
    }
}

/**
 * Called from threads: thread5_game_loop
 */
void enable_background_sound(void) {
    if (WORLD(sBgMusicDisabled) == TRUE) {
        WORLD(sBgMusicDisabled) = FALSE;
        sound_banks_enable(SEQ_PLAYER_SFX, SOUND_BANKS_BACKGROUND);
    }
}

/**
 * Sets the sound mode
 *
 * Called from threads: thread5_game_loop
 */
void set_sound_mode(u16 soundMode) {
    if (soundMode < 3) {
        audio_set_sound_mode(WORLD(sSoundMenuModeToSoundMode)[soundMode]);
    }
}

/**
 * Wrapper method by menu used to set the sound via flags.
 *
 * Called from threads: thread5_game_loop
 */
void play_menu_sounds(s16 soundMenuFlags) {
    if (soundMenuFlags & SOUND_MENU_FLAG_HANDAPPEAR) {
        play_sound(SOUND_MENU_HAND_APPEAR, WORLD(gGlobalSoundSource));
    } else if (soundMenuFlags & SOUND_MENU_FLAG_HANDISAPPEAR) {
        play_sound(SOUND_MENU_HAND_DISAPPEAR, WORLD(gGlobalSoundSource));
    } else if (soundMenuFlags & SOUND_MENU_FLAG_UNKNOWN1) {
        play_sound(SOUND_MENU_UNK0C, WORLD(gGlobalSoundSource));
    } else if (soundMenuFlags & SOUND_MENU_FLAG_PINCHMARIOFACE) {
        play_sound(SOUND_MENU_PINCH_MARIO_FACE, WORLD(gGlobalSoundSource));
    } else if (soundMenuFlags & SOUND_MENU_FLAG_PINCHMARIOFACE2) {
        play_sound(SOUND_MENU_PINCH_MARIO_FACE, WORLD(gGlobalSoundSource));
    } else if (soundMenuFlags & SOUND_MENU_FLAG_LETGOMARIOFACE) {
        play_sound(SOUND_MENU_LET_GO_MARIO_FACE, WORLD(gGlobalSoundSource));
    } else if (soundMenuFlags & SOUND_MENU_FLAG_CAMERAZOOMIN) {
        play_sound(SOUND_MENU_CAMERA_ZOOM_IN, WORLD(gGlobalSoundSource));
    } else if (soundMenuFlags & SOUND_MENU_FLAG_CAMERAZOOMOUT) {
        play_sound(SOUND_MENU_CAMERA_ZOOM_OUT, WORLD(gGlobalSoundSource));
    }

    if (soundMenuFlags & 0x100) {
        play_menu_sounds_extra(20, NULL);
    }
#if ENABLE_RUMBLE
    if (soundMenuFlags & SOUND_MENU_FLAG_LETGOMARIOFACE) {
        queue_rumble_data(10, 60);
    }
#endif
}

/**
 * Plays the painting eject sound effect if it has not already been played
 *
 * Called from threads: thread5_game_loop
 */
void play_painting_eject_sound(void) {
    if (WORLD(gRipplingPainting) != NULL && WORLD(gRipplingPainting)->state == PAINTING_ENTERED) {
        // ripple when Mario enters painting
        if (!WORLD(sPaintingEjectSoundPlayed)) {
            play_sound(SOUND_GENERAL_PAINTING_EJECT,
                       WORLD(gMarioStates)[0].marioObj->header.gfx.cameraToObject);
        }
        WORLD(sPaintingEjectSoundPlayed) = TRUE;
    } else {
        WORLD(sPaintingEjectSoundPlayed) = FALSE;
    }
}

/**
 * Called from threads: thread5_game_loop
 */
void play_infinite_stairs_music(void) {
    u8 shouldPlay = FALSE;

    /* Infinite stairs? */
    if (WORLD(gCurrLevelNum) == LEVEL_CASTLE && WORLD(gCurrAreaIndex) == 2 && WORLD(gMarioState)->numStars < 70) {
        if (WORLD(gMarioState)->floor != NULL && WORLD(gMarioState)->floor->room == 6) {
            if (WORLD(gMarioState)->pos[2] < 2540.0f) {
                shouldPlay = TRUE;
            }
        }
    }

    if (WORLD(sPlayingInfiniteStairs) ^ shouldPlay) {
        WORLD(sPlayingInfiniteStairs) = shouldPlay;
        if (shouldPlay) {
            play_secondary_music(SEQ_EVENT_ENDLESS_STAIRS, 0, 255, 1000);
        } else {
            func_80321080(500);
        }
    }
}

/**
 * Called from threads: thread5_game_loop
 */
void set_background_music(u16 a, u16 seqArgs, s16 fadeTimer) {
    if (WORLD(gResetTimer) == 0 && seqArgs != WORLD(sCurrentMusic)) {
        if (WORLD(gCurrCreditsEntry) != NULL) {
            sound_reset(7);
        } else {
            sound_reset(a);
        }

        if (!WORLD(gNeverEnteredCastle) || seqArgs != SEQ_LEVEL_INSIDE_CASTLE) {
            play_music(SEQ_PLAYER_LEVEL, seqArgs, fadeTimer);
            WORLD(sCurrentMusic) = seqArgs;
        }
    }
}

/**
 * Called from threads: thread3_main, thread5_game_loop
 */
void fadeout_music(s16 fadeOutTime) {
    func_803210D4(fadeOutTime);
    WORLD(sCurrentMusic) = MUSIC_NONE;
    WORLD(sCurrentShellMusic) = MUSIC_NONE;
    WORLD(sCurrentCapMusic) = MUSIC_NONE;
}

/**
 * Called from threads: thread5_game_loop
 */
void fadeout_level_music(s16 fadeTimer) {
    seq_player_fade_out(SEQ_PLAYER_LEVEL, fadeTimer);
    WORLD(sCurrentMusic) = MUSIC_NONE;
    WORLD(sCurrentShellMusic) = MUSIC_NONE;
    WORLD(sCurrentCapMusic) = MUSIC_NONE;
}

/**
 * Called from threads: thread5_game_loop
 */
void play_cutscene_music(u16 seqArgs) {
    play_music(SEQ_PLAYER_LEVEL, seqArgs, 0);
    WORLD(sCurrentMusic) = seqArgs;
}

/**
 * Called from threads: thread5_game_loop
 */
void play_shell_music(void) {
    play_music(SEQ_PLAYER_LEVEL, SEQUENCE_ARGS(4, SEQ_EVENT_POWERUP | SEQ_VARIATION), 0);
    WORLD(sCurrentShellMusic) = SEQUENCE_ARGS(4, SEQ_EVENT_POWERUP | SEQ_VARIATION);
}

/**
 * Called from threads: thread5_game_loop
 */
void stop_shell_music(void) {
    if (WORLD(sCurrentShellMusic) != MUSIC_NONE) {
        stop_background_music(WORLD(sCurrentShellMusic));
        WORLD(sCurrentShellMusic) = MUSIC_NONE;
    }
}

/**
 * Called from threads: thread5_game_loop
 */
void play_cap_music(u16 seqArgs) {
    play_music(SEQ_PLAYER_LEVEL, seqArgs, 0);
    if (WORLD(sCurrentCapMusic) != MUSIC_NONE && WORLD(sCurrentCapMusic) != seqArgs) {
        stop_background_music(WORLD(sCurrentCapMusic));
    }
    WORLD(sCurrentCapMusic) = seqArgs;
}

/**
 * Called from threads: thread5_game_loop
 */
void fadeout_cap_music(void) {
    if (WORLD(sCurrentCapMusic) != MUSIC_NONE) {
        fadeout_background_music(WORLD(sCurrentCapMusic), 600);
    }
}

/**
 * Called from threads: thread5_game_loop
 */
void stop_cap_music(void) {
    if (WORLD(sCurrentCapMusic) != MUSIC_NONE) {
        stop_background_music(WORLD(sCurrentCapMusic));
        WORLD(sCurrentCapMusic) = MUSIC_NONE;
    }
}

/**
 * Called from threads: thread5_game_loop
 */
void play_menu_sounds_extra(s32 a, void *b) {
    play_sound(WORLD(sMenuSoundsExtra)[a], b);
}

/**
 * Called from threads: thread5_game_loop
 */
void audio_game_loop_tick(void) {
    audio_signal_game_loop_tick();
}

/**
 * Sound processing thread. Runs at 60 FPS.
 */
void thread4_sound(UNUSED void *arg) {
    audio_init();
    sound_init();

    // Zero-out unused vector
    vec3f_copy(WORLD(unused80339DC0), WORLD(gVec3fZero));

    osCreateMesgQueue(&WORLD(sSoundMesgQueue), WORLD(sSoundMesgBuf), ARRAY_COUNT(WORLD(sSoundMesgBuf)));
    set_vblank_handler(1, &WORLD(sSoundVblankHandler), &WORLD(sSoundMesgQueue), (OSMesg) 512);

    while (TRUE) {
        OSMesg msg;

        osRecvMesg(&WORLD(sSoundMesgQueue), &msg, OS_MESG_BLOCK);
        if (WORLD(gResetTimer) < 25) {
            struct SPTask *spTask;
            profiler_log_thread4_time();
            spTask = create_next_audio_frame_task();
            if (spTask != NULL) {
                dispatch_audio_sptask(spTask);
            }
            profiler_log_thread4_time();
        }
    }
}
