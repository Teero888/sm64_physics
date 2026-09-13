#ifndef SM64_PHYSICS_OBJECT_FRAME_STATE_H
#define SM64_PHYSICS_OBJECT_FRAME_STATE_H
#include "host/terrain_state.h"
#include "host/scheduler_state.h"
#include "game/platform_displacement.h"
#include "game/object_collision.h"
#define gObjectListArray (sm64_active_objects->lists)
#define gPrevFrameObjectCount (sm64_active_objects->previous_object_count)
#define gCheckingSurfaceCollisionsForCamera (sm64_active_terrain->camera)
/* Profiling and on-screen debug are host services, not simulation branches.
 * Retain the counters reset by the original diagnostic reset. */
u64 sm64_frame_clock(void);
u64 sm64_frame_elapsed(u64 start);
void sm64_frame_reset_diagnostics(void);
void sm64_frame_debug_output(void);
void stub_debug_5(void);
#define get_current_clock sm64_frame_clock
#define get_clock_difference sm64_frame_elapsed
#define reset_debug_objectinfo sm64_frame_reset_diagnostics
#define try_print_debug_mario_object_info sm64_frame_debug_output
#endif
