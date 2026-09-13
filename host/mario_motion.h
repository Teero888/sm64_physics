#ifndef SM64_PHYSICS_MARIO_MOTION_H
#define SM64_PHYSICS_MARIO_MOTION_H
/* Declarations for original functions separated from their previous C unit. */
struct MarioState;
void mario_reset_bodystate(struct MarioState *mario);
void sink_mario_in_quicksand(struct MarioState *mario);
void update_mario_health(struct MarioState *mario);
void update_mario_info_for_cam(struct MarioState *mario);
void mario_update_hitbox_and_cap_model(struct MarioState *mario);
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
