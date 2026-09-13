#include <assert.h>
#include <stdio.h>
#include "../host/audio.h"
#include "sm64.h"
#include "engine/graph_node.h"
#include "audio/external.h"
#include "game/sound_init.h"
#include "game/mario.h"
#include "game/spawn_sound.h"
#include "seq_ids.h"

struct Object *gCurrentObject;
u32 gAudioRandom;
struct log { struct sm64_audio_event events[64]; size_t count; };
static void record(void *user, const struct sm64_audio_event *event) {
    struct log *log = user;
    assert(log->count < 64);
    log->events[log->count++] = *event;
}

int main(void) {
    struct log first = {0}, second = {0};
    struct sm64_audio_state *a = sm64_audio_create(record, &first);
    struct sm64_audio_state *b = sm64_audio_create(record, &second);
    assert(a && b);
    assert(sm64_audio_activate(a) == NULL);
    play_cap_music(123);
    play_cap_music(456);
    assert(first.count == 3);
    assert(first.events[0].command == SM64_AUDIO_PLAY_MUSIC);
    assert(first.events[1].args[1] == 456);
    assert(first.events[2].command == SM64_AUDIO_STOP_MUSIC && first.events[2].args[0] == 123);
    fadeout_cap_music();
    assert(first.events[3].command == SM64_AUDIO_FADE_MUSIC);
    assert(first.events[3].args[0] == 456 && first.events[3].args[1] == 600);

    assert(sm64_audio_activate(b) == a);
    stop_cap_music();
    assert(second.count == 0);
    play_shell_music();
    stop_shell_music();
    stop_shell_music();
    assert(second.count == 2);
    assert(second.events[1].command == SM64_AUDIO_STOP_MUSIC);
    assert(second.events[1].args[0] == SEQUENCE_ARGS(4, SEQ_EVENT_POWERUP | SEQ_VARIATION));

    struct sm64_audio_state *copy = sm64_audio_clone(a, record, &second);
    assert(copy);
    sm64_audio_activate(copy);
    stop_cap_music();
    assert(second.count == 3 && second.events[2].args[0] == 456);
    sm64_audio_destroy(copy);
    sm64_audio_activate(a);
    stop_cap_music();
    assert(first.count == 5 && first.events[4].args[0] == 456);
    disable_background_sound();
    disable_background_sound();
    enable_background_sound();
    enable_background_sound();
    assert(first.count == 7);
    assert(first.events[5].command == SM64_AUDIO_DISABLE_BANKS);
    assert(first.events[6].command == SM64_AUDIO_ENABLE_BANKS);

    struct Object object = {0};
    gCurrentObject = &object;
    object.header.gfx.cameraToObject[0] = 10;
    object.header.gfx.cameraToObject[1] = 20;
    object.header.gfx.cameraToObject[2] = 30;
    cur_obj_play_sound_2((s32)0xf1234567u);
    assert(first.count == 7);
    object.header.gfx.node.flags = GRAPH_RENDER_ACTIVE;
    cur_obj_play_sound_2((s32)0xf1234567u);
    assert(first.count == 8);
    assert(first.events[7].args[0] == 0xf1234567u);
    assert(first.events[7].position[0] == 10 && first.events[7].position[2] == 30);
    assert(first.events[7].source == (uintptr_t)object.header.gfx.cameraToObject);
    object.header.gfx.cameraToObject[0] = 99;
    assert(first.events[7].position[0] == 10);
    stop_sounds_from_source(object.header.gfx.cameraToObject);
    assert(first.events[8].source == first.events[7].source);

    struct SoundState sounds[] = {{TRUE, 2, 5, 0x12345678}};
    object.header.gfx.animInfo.animFrame = 1;
    exec_anim_sound_state(sounds);
    assert(first.count == 9);
    object.header.gfx.animInfo.animFrame = 2;
    exec_anim_sound_state(sounds);
    object.header.gfx.animInfo.animFrame = 5;
    exec_anim_sound_state(sounds);
    assert(first.count == 11 && first.events[10].args[0] == 0x12345678);

    play_cap_music(789);
    fadeout_level_music(30);
    size_t count = first.count;
    stop_cap_music();
    assert(first.count == count);
    assert(first.events[count - 1].command == SM64_AUDIO_FADE_PLAYER);
    assert(first.events[count - 1].args[1] == 30);

    struct MarioState mario = {.marioObj = &object, .action = ACT_TRIPLE_JUMP};
    gAudioRandom = 4;
    play_mario_jump_sound(&mario);
    play_mario_jump_sound(&mario);
    assert(first.count == count + 1);
    assert(first.events[count].args[0] == SOUND_MARIO_YAHOO_WAHA_YIPPEE + (4u << 16));
    assert(mario.flags & MARIO_MARIO_SOUND_PLAYED);
    mario.terrainSoundAddend = SOUND_TERRAIN_WATER << 16;
    play_mario_landing_sound_once(&mario, SOUND_ACTION_TERRAIN_LANDING);
    play_mario_landing_sound_once(&mario, SOUND_ACTION_TERRAIN_LANDING);
    assert(first.count == count + 2);
    assert(mario.particleFlags & PARTICLE_SHALLOW_WATER_SPLASH);
    assert(mario.flags & MARIO_ACTION_SOUND_PLAYED);
    mario.flags = MARIO_METAL_CAP;
    play_mario_heavy_landing_sound(&mario, SOUND_ACTION_TERRAIN_HEAVY_LANDING);
    assert(first.events[count + 2].args[0] == SOUND_ACTION_METAL_HEAVY_LANDING);
    mario.forwardVel = -150;
    adjust_sound_for_speed(&mario);
    assert(first.events[count + 3].command == SM64_AUDIO_MOVING_SPEED);
    assert(first.events[count + 3].args[1] == 100);
    sm64_audio_destroy(a);
    sm64_audio_destroy(b);
    puts("Native audio events: original music transitions, object timing, source copies and independent state passed");
    return 0;
}
