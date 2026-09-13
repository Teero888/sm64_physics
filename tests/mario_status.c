#include <assert.h>
#include <stdio.h>
#include "../host/mario_status.h"
#include "../host/audio.h"
#include "game/camera.h"
#include "game/area.h"
#include "engine/graph_node.h"
int main(void) {
    struct sm64_objects *objects = sm64_objects_create();
    struct sm64_audio_state *audio = sm64_audio_create(NULL, NULL);
    assert(objects && audio);
    sm64_objects_activate(objects);
    sm64_audio_activate(audio);
    struct Object object = {0};
    struct Area area = {.terrainType = TERRAIN_GRASS};
    struct MarioBodyState body = {0};
    struct PlayerCameraState camera = {0};
    struct MarioState mario = {.marioObj = &object, .area = &area, .marioBodyState = &body,
        .statusForCamera = &camera, .health = 0x880, .action = ACT_IDLE};
    sm64_objects_set_motion_state(objects, &mario, NULL, 0);
    mario.input = INPUT_IN_POISON_GAS;
    update_mario_health(&mario);
    assert(mario.health == 0x87c);
    mario.flags = MARIO_METAL_CAP;
    update_mario_health(&mario);
    assert(mario.health == 0x87c);
    mario.flags = 0;
    mario.healCounter = mario.hurtCounter = 1;
    update_mario_health(&mario);
    assert(mario.health == 0x87c && !mario.healCounter && !mario.hurtCounter);
    mario.input = 0;
    mario.action = ACT_WATER_IDLE;
    mario.health = 0x500;
    mario.waterLevel = 1000;
    mario.pos[1] = 860;
    update_mario_health(&mario);
    assert(mario.health == 0x51a);
    mario.pos[1] = 859;
    update_mario_health(&mario);
    assert(mario.health == 0x519);
    area.terrainType = TERRAIN_SNOW;
    update_mario_health(&mario);
    assert(mario.health == 0x516);
    sm64_objects_set_frame_info(objects, 0, 1);
    update_mario_health(&mario);
    assert(mario.health == 0x516);
    sm64_objects_set_frame_info(objects, 1, 0);
    mario.health = 0x100;
    update_mario_health(&mario);
    assert(mario.health == 0xff);
    mario.healCounter = 1;
    update_mario_health(&mario);
    assert(mario.health == 0xff && mario.healCounter == 1);

    mario.flags = MARIO_WING_CAP | MARIO_CAP_ON_HEAD;
    mario.action = ACT_READING_SIGN;
    mario.capTimer = 61;
    update_and_return_cap_flags(&mario);
    assert(mario.capTimer == 61);
    mario.capTimer = 60;
    update_and_return_cap_flags(&mario);
    assert(mario.capTimer == 59);
    mario.capTimer = 1;
    update_and_return_cap_flags(&mario);
    assert(mario.capTimer == 0 && !(mario.flags & MARIO_SPECIAL_CAPS));
    assert(!(mario.flags & MARIO_CAP_ON_HEAD));
    mario.flags = MARIO_NORMAL_CAP | MARIO_METAL_CAP | MARIO_CAP_ON_HEAD;
    mario.capTimer = 1;
    update_and_return_cap_flags(&mario);
    assert(mario.flags & MARIO_NORMAL_CAP);
    assert(mario.flags & MARIO_CAP_ON_HEAD);
    mario.action = ACT_CROUCHING;
    mario.invincTimer = 3;
    mario_reset_bodystate(&mario);
    mario_update_hitbox_and_cap_model(&mario);
    assert(object.hitboxHeight == 100);
    assert(object.header.gfx.node.flags & GRAPH_RENDER_INVISIBLE);
    mario.action = ACT_IDLE;
    mario_update_hitbox_and_cap_model(&mario);
    assert(object.hitboxHeight == 160);
    mario.pos[0] = 123;
    mario.faceAngle[1] = 456;
    update_mario_info_for_cam(&mario);
    assert(camera.pos[0] == 123 && camera.faceAngle[1] == 456 && body.action == ACT_IDLE);
    mario.flags |= MARIO_UNKNOWN_25;
    mario.pos[0] = 789;
    update_mario_info_for_cam(&mario);
    assert(camera.pos[0] == 123);
    object.header.gfx.pos[1] = 100;
    mario.quicksandDepth = 20;
    sink_mario_in_quicksand(&mario);
    assert(object.header.gfx.pos[1] == 80);
    sm64_objects_destroy(objects);
    sm64_audio_destroy(audio);
    puts("Original Mario status: environmental health, cap expiry, hitboxes and body/camera output passed");
    return 0;
}
