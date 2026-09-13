/* Generated verbatim upstream function extraction. See extracted.json. */
#include "host/audio_state.h"

#line 35 "n64decomp/src/game/sound_init.c"
static s16 sSoundMenuModeToSoundMode[] = { SOUND_MODE_STEREO, SOUND_MODE_MONO, SOUND_MODE_HEADSET };

#line 141 "n64decomp/src/game/sound_init.c"
void set_sound_mode(u16 soundMode) {
    if (soundMode < 3) {
        audio_set_sound_mode(sSoundMenuModeToSoundMode[soundMode]);
    }
}

#line 82 "n64decomp/src/game/sound_init.c"
void reset_volume(void) {
    sMusicVolume = 0;
}

#line 89 "n64decomp/src/game/sound_init.c"
void lower_background_noise(s32 a) {
    switch (a) {
        case 1:
            set_audio_muted(TRUE);
            break;
        case 2:
            seq_player_lower_volume(SEQ_PLAYER_LEVEL, 60, 40);
            break;
    }
    sMusicVolume |= a;
}

#line 104 "n64decomp/src/game/sound_init.c"
void raise_background_noise(s32 a) {
    switch (a) {
        case 1:
            set_audio_muted(FALSE);
            break;
        case 2:
            seq_player_unlower_volume(SEQ_PLAYER_LEVEL, 60);
            break;
    }
    sMusicVolume &= ~a;
}

#line 119 "n64decomp/src/game/sound_init.c"
void disable_background_sound(void) {
    if (sBgMusicDisabled == FALSE) {
        sBgMusicDisabled = TRUE;
        sound_banks_disable(SEQ_PLAYER_SFX, SOUND_BANKS_BACKGROUND);
    }
}

#line 129 "n64decomp/src/game/sound_init.c"
void enable_background_sound(void) {
    if (sBgMusicDisabled == TRUE) {
        sBgMusicDisabled = FALSE;
        sound_banks_enable(SEQ_PLAYER_SFX, SOUND_BANKS_BACKGROUND);
    }
}

#line 245 "n64decomp/src/game/sound_init.c"
void fadeout_music(s16 fadeOutTime) {
    func_803210D4(fadeOutTime);
    sCurrentMusic = MUSIC_NONE;
    sCurrentShellMusic = MUSIC_NONE;
    sCurrentCapMusic = MUSIC_NONE;
}

#line 255 "n64decomp/src/game/sound_init.c"
void fadeout_level_music(s16 fadeTimer) {
    seq_player_fade_out(SEQ_PLAYER_LEVEL, fadeTimer);
    sCurrentMusic = MUSIC_NONE;
    sCurrentShellMusic = MUSIC_NONE;
    sCurrentCapMusic = MUSIC_NONE;
}

#line 265 "n64decomp/src/game/sound_init.c"
void play_cutscene_music(u16 seqArgs) {
    play_music(SEQ_PLAYER_LEVEL, seqArgs, 0);
    sCurrentMusic = seqArgs;
}

#line 273 "n64decomp/src/game/sound_init.c"
void play_shell_music(void) {
    play_music(SEQ_PLAYER_LEVEL, SEQUENCE_ARGS(4, SEQ_EVENT_POWERUP | SEQ_VARIATION), 0);
    sCurrentShellMusic = SEQUENCE_ARGS(4, SEQ_EVENT_POWERUP | SEQ_VARIATION);
}

#line 281 "n64decomp/src/game/sound_init.c"
void stop_shell_music(void) {
    if (sCurrentShellMusic != MUSIC_NONE) {
        stop_background_music(sCurrentShellMusic);
        sCurrentShellMusic = MUSIC_NONE;
    }
}

#line 291 "n64decomp/src/game/sound_init.c"
void play_cap_music(u16 seqArgs) {
    play_music(SEQ_PLAYER_LEVEL, seqArgs, 0);
    if (sCurrentCapMusic != MUSIC_NONE && sCurrentCapMusic != seqArgs) {
        stop_background_music(sCurrentCapMusic);
    }
    sCurrentCapMusic = seqArgs;
}

#line 302 "n64decomp/src/game/sound_init.c"
void fadeout_cap_music(void) {
    if (sCurrentCapMusic != MUSIC_NONE) {
        fadeout_background_music(sCurrentCapMusic, 600);
    }
}

#line 311 "n64decomp/src/game/sound_init.c"
void stop_cap_music(void) {
    if (sCurrentCapMusic != MUSIC_NONE) {
        stop_background_music(sCurrentCapMusic);
        sCurrentCapMusic = MUSIC_NONE;
    }
}
