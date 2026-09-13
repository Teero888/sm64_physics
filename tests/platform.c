#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../host/objects.h"
#include "../host/terrain.h"
#include "game/platform_displacement.h"
#include "game/object_list_processor.h"

int main(void) {
    struct sm64_objects *objects = sm64_objects_create();
    struct Object mario_object = {0}, platform = {0}, rider = {0};
    struct MarioState mario = {0};
    struct sm64_terrain_triangle triangle = {
        .vertices = {{-1000, 0, -1000}, {0, 0, 1000}, {1000, 0, -1000}}, .dynamic = true};
    struct sm64_terrain *terrain = sm64_terrain_create(&triangle, 1, NULL, 0);
    assert(objects && terrain);
    sm64_objects_activate(objects);
    sm64_objects_set_mario(objects, &mario_object);
    sm64_objects_set_motion_state(objects, &mario, &rider, 0);
    sm64_terrain_activate(terrain);
    struct Surface *floor;
    assert(find_floor(0, 0, 0, &floor) == 0 && floor);
    floor->object = &platform;
    update_mario_platform();
    assert(mario_object.platform == &platform);
    platform.oVelX = 5;
    platform.oVelY = 50;
    platform.oVelZ = -3;
    apply_mario_platform_displacement();
    assert(mario.pos[0] == 5 && mario.pos[1] == 0 && mario.pos[2] == -3);
    sm64_objects_set_motion_state(objects, &mario, &rider, TIME_STOP_ACTIVE);
    apply_mario_platform_displacement();
    assert(mario.pos[0] == 5);
    sm64_objects_set_motion_state(objects, &mario, &rider, 0);
    mario_object.oPosY = 4;
    update_mario_platform();
    assert(mario_object.platform == NULL);
    apply_mario_platform_displacement();
    assert(mario.pos[0] == 5);
    mario_object.oPosY = 3.99f;
    update_mario_platform();
    assert(mario_object.platform == &platform);

    platform.oVelX = platform.oVelZ = 0;
    platform.oFaceAngleYaw = platform.oAngleVelYaw = 0x4000;
    set_mario_pos(100, 0, 0);
    apply_mario_platform_displacement();
    assert(fabsf(mario.pos[0]) < 0.001f && fabsf(mario.pos[2] + 100) < 0.001f);
    assert(mario.faceAngle[1] == 0x4000);
    rider.oPosX = 100;
    apply_platform_displacement(FALSE, &platform);
    assert(fabsf(rider.oPosX) < 0.001f && fabsf(rider.oPosZ + 100) < 0.001f);
    assert(rider.oFaceAngleYaw == 0); // only Mario's facing follows rotation
    clear_mario_platform();
    apply_mario_platform_displacement();
    assert(mario.faceAngle[1] == 0x4000);

    struct sm64_objects *other = sm64_objects_create();
    assert(other);
    sm64_objects_activate(other);
    struct MarioState second = {0};
    struct Object second_object = {0};
    sm64_objects_set_mario(other, &second_object);
    sm64_objects_set_motion_state(other, &second, NULL, 0);
    apply_mario_platform_displacement();
    assert(second.faceAngle[1] == 0);
    update_mario_platform();
    apply_mario_platform_displacement();
    assert(second.faceAngle[1] == 0x4000);
    assert(mario.faceAngle[1] == 0x4000);
    sm64_objects_destroy(objects);
    floor->object = NULL;
    update_mario_platform();
    assert(second_object.platform == NULL);
    sm64_objects_destroy(other);
    sm64_terrain_destroy(terrain);
    puts("Native platform displacement: tracking, thresholds, translation, rotation, time stop and independent owners passed");
    return 0;
}
