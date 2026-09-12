#ifndef SM64_PHYSICS_MARIO_MOTION_H
#define SM64_PHYSICS_MARIO_MOTION_H
/* Declarations for original functions separated from their previous C unit. */
struct MarioState;
void apply_gravity(struct MarioState *mario);
void update_mario_button_inputs(struct MarioState *mario);
void update_mario_joystick_inputs(struct MarioState *mario);
#endif
