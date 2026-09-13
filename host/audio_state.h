#ifndef SM64_PHYSICS_AUDIO_STATE_H
#define SM64_PHYSICS_AUDIO_STATE_H
#include "sm64.h"
#include "audio/external.h"
#include "game/sound_init.h"
#include "seq_ids.h"
#include "host/audio.h"

struct sm64_audio_state {
    sm64_audio_sink sink;
    void *user;
    u8 music_volume, bg_disabled;
    u16 current_music, shell_music, cap_music;
};
extern _Thread_local struct sm64_audio_state *sm64_active_audio;
#define MUSIC_NONE 0xFFFF
#define sMusicVolume (sm64_active_audio->music_volume)
#define sBgMusicDisabled (sm64_active_audio->bg_disabled)
#define sCurrentMusic (sm64_active_audio->current_music)
#define sCurrentShellMusic (sm64_active_audio->shell_music)
#define sCurrentCapMusic (sm64_active_audio->cap_music)
#endif
