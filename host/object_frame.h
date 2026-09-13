#ifndef SM64_PHYSICS_OBJECT_FRAME_H
#define SM64_PHYSICS_OBJECT_FRAME_H
#include "objects.h"
#include "terrain.h"
/* Runs original update_objects in full. Caller activates audio and supplies
 * valid native behaviors/assets. Mario execution requires its real callback;
 * this function does not synthesize Mario actions or animation assets.
 * Increments the owned 16-bit animation tick before behaviors and advances
 * active objects' animation state afterwards, with scheduler freeze flags. */
void sm64_objects_step(struct sm64_objects *objects, struct sm64_terrain *terrain, u32 frame);
u32 sm64_objects_time_stop(const struct sm64_objects *objects);
void sm64_objects_set_time_stop(struct sm64_objects *objects, u32 flags);
u32 sm64_objects_previous_count(const struct sm64_objects *objects);
#endif
