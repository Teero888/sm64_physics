#include <assert.h>
#include "../host/behavior.h"
#include "../host/audio.h"
#include "../host/mario_status.h"
#include "game/interaction.h"
#include "game/area.h"
#include "game/object_helpers.h"


static const BehaviorScript held[] = {0}, dropped[] = {0}, thrown[] = {0}, shell[] = {0};
int main(void) {
    struct sm64_objects *objects = sm64_objects_create();
    struct sm64_audio_state *audio = sm64_audio_create(NULL, NULL);
    assert(objects && audio);
    sm64_objects_activate(objects);
    sm64_audio_activate(audio);
    sm64_objects_bind_carry_assets(objects, held, dropped, thrown, shell);
    struct Object player = {0}, item = {0}, ride = {0};
    struct MarioBodyState body = {.heldObjLastPosition = {10, 200, 30}};
    struct Area area = {.terrainType = TERRAIN_GRASS};
    struct Surface floor = {.normal = {0, 1, 0}};
    struct MarioState m = {.marioObj = &player, .marioBodyState = &body,
        .usedObj = &item, .area = &area, .floor = &floor, .action = ACT_IDLE};
    sm64_objects_set_motion_state(objects, &m, &player, 0);
    item.oFlags = OBJ_FLAG_HOLDABLE;
    mario_grab_used_object(&m);
    assert(m.heldObj == &item && item.oHeldState == HELD_HELD && item.parentObj == &player);
    m.pos[1] = 50;
    m.faceAngle[1] = 0;
    mario_drop_held_object(&m);
    assert(!m.heldObj && item.oHeldState == HELD_DROPPED);
    assert(item.oPosX == 10 && item.oPosY == 50 && item.oPosZ == 30);
    mario_grab_used_object(&m);
    mario_throw_held_object(&m);
    assert(!m.heldObj && item.oHeldState == HELD_THROWN);
    assert(item.oPosX == 10 && item.oPosY == 200 && item.oPosZ == 62);
    item.oFlags = 0;
    item.bhvStackIndex = 3;
    mario_grab_used_object(&m);
    assert(item.curBhvCommand == held && item.bhvStackIndex == 0);
    m.action = ACT_RIDING_HOOT;
    m.riddenObj = &ride;
    sm64_objects_set_frame_info(objects, 123, 0);
    drop_and_set_mario_action(&m, ACT_FREEFALL, 7);
    assert(!m.heldObj && !m.riddenObj && m.action == ACT_FREEFALL && m.actionArg == 7);
    assert(item.curBhvCommand == dropped && item.oHootMarioReleaseTime == 123);
    assert(ride.oInteractStatus == INT_STATUS_STOP_RIDING);
    m.action = ACT_IDLE;
    m.input = INPUT_A_PRESSED | INPUT_OFF_FLOOR | INPUT_NONZERO_ANALOG;
    assert(check_common_action_exits(&m) && m.action == ACT_JUMP);
    m.prevAction = ACT_DOUBLE_JUMP_LAND;
    m.doubleJumpTimer = 5;
    m.forwardVel = 21;
    assert(set_jump_from_landing(&m) && m.action == ACT_TRIPLE_JUMP && !m.doubleJumpTimer);
    m.quicksandDepth = 11;
    m.heldObj = &item;
    assert(set_jumping_action(&m, ACT_JUMP, 0) && m.action == ACT_HOLD_QUICKSAND_JUMP_LAND);
    sm64_audio_activate(NULL);
    sm64_objects_activate(NULL);
    sm64_audio_destroy(audio);
    sm64_objects_destroy(objects);
    return 0;
}
