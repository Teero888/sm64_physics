#ifndef SM64_PHYSICS_CONTROLLER_H
#define SM64_PHYSICS_CONTROLLER_H
#include <stdint.h>
#include "types.h"
/* The caller owns this Controller as part of its simulation state. */
void sm64_controller_update(struct Controller *controller, uint16_t buttons, int8_t x, int8_t y);
void adjust_analog_stick(struct Controller *controller);
void update_mario_button_inputs(struct MarioState *mario);
void update_mario_joystick_inputs(struct MarioState *mario);
#endif
