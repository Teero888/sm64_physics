#ifndef SM64_PHYSICS_MARIO_MOTION_H
#define SM64_PHYSICS_MARIO_MOTION_H
/* Declarations for original functions separated from their previous C unit. */
struct MarioState;
void apply_gravity(struct MarioState *mario);
void update_mario_button_inputs(struct MarioState *mario);
void update_mario_joystick_inputs(struct MarioState *mario);
void update_mario_geometry_inputs(struct MarioState *mario);
void update_mario_inputs(struct MarioState *mario);
void debug_print_speed_action_normal(struct MarioState *mario);
void update_terrain_objects(void);
void update_non_terrain_objects(void);
void unload_deactivated_objects(void);
#endif
