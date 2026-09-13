#ifndef SM64_PHYSICS_MARIO_STATUS_H
#define SM64_PHYSICS_MARIO_STATUS_H
#include "objects.h"
#include "game/mario.h"
/* Original per-frame updates; require active object/audio contexts and valid
 * Mario body, object, area and camera-status pointers for the routines used. */
void update_mario_health(struct MarioState *m);
void update_mario_info_for_cam(struct MarioState *m);
void mario_reset_bodystate(struct MarioState *m);
void sink_mario_in_quicksand(struct MarioState *m);
u32 update_and_return_cap_flags(struct MarioState *m);
void mario_update_hitbox_and_cap_model(struct MarioState *m);
void sm64_objects_set_frame_info(struct sm64_objects *objects, u32 frame, u8 debug_level_select);
#endif
