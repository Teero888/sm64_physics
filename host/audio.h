#ifndef SM64_PHYSICS_AUDIO_H
#define SM64_PHYSICS_AUDIO_H
#include <stdint.h>

enum sm64_audio_command {
    SM64_AUDIO_PLAY_SOUND, SM64_AUDIO_STOP_SOUND, SM64_AUDIO_STOP_SOURCE,
    SM64_AUDIO_FADE_PLAYER, SM64_AUDIO_FADE_VOLUME, SM64_AUDIO_LOWER_VOLUME,
    SM64_AUDIO_RESTORE_VOLUME, SM64_AUDIO_MUTE, SM64_AUDIO_DISABLE_BANKS,
    SM64_AUDIO_ENABLE_BANKS, SM64_AUDIO_MOVING_SPEED, SM64_AUDIO_PLAY_MUSIC,
    SM64_AUDIO_STOP_MUSIC, SM64_AUDIO_FADE_MUSIC, SM64_AUDIO_DROP_MUSIC,
    SM64_AUDIO_SECONDARY_MUSIC, SM64_AUDIO_STOP_SECONDARY, SM64_AUDIO_FADE_ALL,
    SM64_AUDIO_COURSE_CLEAR, SM64_AUDIO_PEACH_JINGLE, SM64_AUDIO_PUZZLE_JINGLE,
    SM64_AUDIO_STAR_FANFARE, SM64_AUDIO_POWER_STAR, SM64_AUDIO_RACE_FANFARE,
    SM64_AUDIO_TOAD_JINGLE
};

struct sm64_audio_event {
    enum sm64_audio_command command;
    /* Arguments retain the order and unsigned bit patterns of the upstream
     * audio/external.h call. Unused arguments are zero. */
    uint32_t args[4];
    /* An opaque source identity, never dereference it. This is a native
     * address cookie, valid only within the current world's lifetime. */
    uintptr_t source;
    /* Copy of upstream's camera-relative source coordinates at emission. */
    float position[3];
};
typedef void (*sm64_audio_sink)(void *user, const struct sm64_audio_event *event);
struct sm64_audio_state;
/* Events are synchronous and valid only during the callback. The sink must
 * copy anything it retains and must not reenter the simulation. A null sink
 * explicitly selects silent simulation without changing music bookkeeping. */
struct sm64_audio_state *sm64_audio_create(sm64_audio_sink sink, void *user);
void sm64_audio_destroy(struct sm64_audio_state *state);
/* All audio calls require an active state. One state may be active on only
 * one thread at a time. Returns the prior state for nested activation. */
struct sm64_audio_state *sm64_audio_activate(struct sm64_audio_state *state);
/* Copies music bookkeeping only; pending presentation events aren't replayed. */
struct sm64_audio_state *sm64_audio_clone(const struct sm64_audio_state *state,
                                        sm64_audio_sink sink, void *user);
#endif
