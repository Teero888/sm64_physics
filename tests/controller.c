#include <stdio.h>
#include <stdlib.h>
#include "sm64.h"
#include "game/area.h"
#include "game/camera.h"
#include "../host/controller.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

static void read_input(struct MarioState *m) {
    m->input = 0;
    update_mario_button_inputs(m);
    update_mario_joystick_inputs(m);
}

int main(void) {
    struct Controller controller = {0}, independent = {0};
    struct Camera camera = {0};
    struct Area area = {.camera = &camera};
    struct MarioState mario = {.controller = &controller, .area = &area,
                               .framesSinceA = 255, .framesSinceB = 255};
    mario.faceAngle[1] = 1234;
    sm64_controller_update(&controller, 0, 7, -7);
    CHECK(controller.stickMag == 0 && controller.stickX == 0 && controller.stickY == 0);
    read_input(&mario);
    CHECK(mario.intendedMag == 0 && mario.intendedYaw == 1234);
    CHECK(mario.framesSinceA == 255 && mario.framesSinceB == 255);
    sm64_controller_update(&controller, A_BUTTON | B_BUTTON | Z_TRIG, 8, 0);
    CHECK(controller.stickX == 2 && controller.stickMag == 2);
    read_input(&mario);
    CHECK(mario.intendedMag == 0.03125f);
    CHECK((mario.input & (INPUT_A_PRESSED | INPUT_A_DOWN | INPUT_B_PRESSED | INPUT_Z_DOWN | INPUT_Z_PRESSED))
          == (INPUT_A_PRESSED | INPUT_A_DOWN | INPUT_B_PRESSED | INPUT_Z_DOWN | INPUT_Z_PRESSED));
    CHECK(mario.framesSinceA == 0 && mario.framesSinceB == 0);
    sm64_controller_update(&controller, A_BUTTON | B_BUTTON | Z_TRIG, 127, 127);
    CHECK(controller.buttonPressed == 0 && controller.stickMag == 64);
    CHECK(controller.stickX == controller.stickY);
    read_input(&mario);
    CHECK(!(mario.input & (INPUT_A_PRESSED | INPUT_B_PRESSED | INPUT_Z_PRESSED)));
    CHECK(mario.intendedMag == 32 && mario.framesSinceA == 1 && mario.framesSinceB == 1);
    s16 yaw = mario.intendedYaw;
    camera.yaw = 0x2000;
    read_input(&mario);
    CHECK((s16)(mario.intendedYaw - yaw) == 0x2000);
    sm64_controller_update(&controller, 0, 0, 0);
    sm64_controller_update(&controller, A_BUTTON | B_BUTTON | Z_TRIG, -128, 0);
    mario.squishTimer = 1;
    read_input(&mario);
    CHECK(fabsf(controller.stickX + 64) < 0.00001f && controller.stickMag == 64);
    CHECK(mario.intendedMag == 8);
    CHECK(mario.input & INPUT_A_PRESSED);
    CHECK(!(mario.input & (INPUT_B_PRESSED | INPUT_Z_DOWN | INPUT_Z_PRESSED)));
    sm64_controller_update(&independent, A_BUTTON, 0, 0);
    CHECK(independent.buttonPressed == A_BUTTON);
    sm64_controller_update(&controller, A_BUTTON, 0, 0);
    CHECK(controller.buttonPressed == 0);
    struct Controller branch = controller;
    sm64_controller_update(&controller, 0, 0, 0);
    sm64_controller_update(&branch, A_BUTTON, 0, 0);
    CHECK(branch.buttonPressed == 0 && branch.buttonDown == A_BUTTON && controller.buttonDown == 0);
    puts("Original controller rules: dead zone, normalization, edges, timers, camera yaw and squish passed");
    return 0;
}
