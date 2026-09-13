#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "audio_state.h"

_Thread_local struct sm64_audio_state *sm64_active_audio;

struct sm64_audio_state *sm64_audio_create(sm64_audio_sink sink, void *user) {
    struct sm64_audio_state *state = calloc(1, sizeof(*state));
    if (state) {
        state->sink = sink;
        state->user = user;
        state->current_music = state->shell_music = state->cap_music = MUSIC_NONE;
    }
    return state;
}

struct sm64_audio_state *sm64_audio_clone(const struct sm64_audio_state *state,
                                        sm64_audio_sink sink, void *user) {
    if (!state) return NULL;
    struct sm64_audio_state *copy = malloc(sizeof(*copy));
    if (copy) {
        *copy = *state;
        copy->sink = sink;
        copy->user = user;
    }
    return copy;
}

void sm64_audio_destroy(struct sm64_audio_state *state) {
    if (sm64_active_audio == state) sm64_active_audio = NULL;
    free(state);
}

struct sm64_audio_state *sm64_audio_activate(struct sm64_audio_state *state) {
    struct sm64_audio_state *previous = sm64_active_audio;
    sm64_active_audio = state;
    return previous;
}

static void emit(enum sm64_audio_command command, u32 a, u32 b, u32 c, u32 d, const f32 *pos) {
    assert(sm64_active_audio);
    struct sm64_audio_event event = { .command = command, .args = {a, b, c, d},
                                     .source = (uintptr_t)pos };
    if (pos) memcpy(event.position, pos, sizeof(event.position));
    if (sm64_active_audio->sink) sm64_active_audio->sink(sm64_active_audio->user, &event);
}

void play_sound(s32 bits, f32 *pos) { emit(SM64_AUDIO_PLAY_SOUND, (u32)bits, 0, 0, 0, pos); }
void stop_sound(u32 bits, f32 *pos) { emit(SM64_AUDIO_STOP_SOUND, bits, 0, 0, 0, pos); }
void stop_sounds_from_source(f32 *pos) { emit(SM64_AUDIO_STOP_SOURCE, 0, 0, 0, 0, pos); }
void seq_player_fade_out(u8 player, u16 duration) { emit(SM64_AUDIO_FADE_PLAYER, player, duration, 0, 0, NULL); }
void fade_volume_scale(u8 player, u8 scale, u16 duration) { emit(SM64_AUDIO_FADE_VOLUME, player, scale, duration, 0, NULL); }
void seq_player_lower_volume(u8 player, u16 duration, u8 percent) { emit(SM64_AUDIO_LOWER_VOLUME, player, duration, percent, 0, NULL); }
void seq_player_unlower_volume(u8 player, u16 duration) { emit(SM64_AUDIO_RESTORE_VOLUME, player, duration, 0, 0, NULL); }
void set_audio_muted(u8 muted) { emit(SM64_AUDIO_MUTE, muted, 0, 0, 0, NULL); }
void sound_banks_disable(u8 player, u16 mask) { emit(SM64_AUDIO_DISABLE_BANKS, player, mask, 0, 0, NULL); }
void sound_banks_enable(u8 player, u16 mask) { emit(SM64_AUDIO_ENABLE_BANKS, player, mask, 0, 0, NULL); }
void set_sound_moving_speed(u8 bank, u8 speed) { emit(SM64_AUDIO_MOVING_SPEED, bank, speed, 0, 0, NULL); }
void play_music(u8 player, u16 args, u16 fade) { emit(SM64_AUDIO_PLAY_MUSIC, player, args, fade, 0, NULL); }
void stop_background_music(u16 id) { emit(SM64_AUDIO_STOP_MUSIC, id, 0, 0, 0, NULL); }
void fadeout_background_music(u16 id, u16 fade) { emit(SM64_AUDIO_FADE_MUSIC, id, fade, 0, 0, NULL); }
void drop_queued_background_music(void) { emit(SM64_AUDIO_DROP_MUSIC, 0, 0, 0, 0, NULL); }
void play_secondary_music(u8 id, u8 bg, u8 volume, u16 fade) { emit(SM64_AUDIO_SECONDARY_MUSIC, id, bg, volume, fade, NULL); }
void func_80321080(u16 fade) { emit(SM64_AUDIO_STOP_SECONDARY, fade, 0, 0, 0, NULL); }
void func_803210D4(u16 fade) { emit(SM64_AUDIO_FADE_ALL, fade, 0, 0, 0, NULL); }
void play_course_clear(void) { emit(SM64_AUDIO_COURSE_CLEAR, 0, 0, 0, 0, NULL); }
void play_peachs_jingle(void) { emit(SM64_AUDIO_PEACH_JINGLE, 0, 0, 0, 0, NULL); }
void play_puzzle_jingle(void) { emit(SM64_AUDIO_PUZZLE_JINGLE, 0, 0, 0, 0, NULL); }
void play_star_fanfare(void) { emit(SM64_AUDIO_STAR_FANFARE, 0, 0, 0, 0, NULL); }
void play_power_star_jingle(u8 arg) { emit(SM64_AUDIO_POWER_STAR, arg, 0, 0, 0, NULL); }
void play_race_fanfare(void) { emit(SM64_AUDIO_RACE_FANFARE, 0, 0, 0, 0, NULL); }
void play_toads_jingle(void) { emit(SM64_AUDIO_TOAD_JINGLE, 0, 0, 0, 0, NULL); }
void audio_set_sound_mode(u8 mode) { emit(SM64_AUDIO_SOUND_MODE, mode, 0, 0, 0, NULL); }
