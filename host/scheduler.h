#ifndef SM64_PHYSICS_SCHEDULER_H
#define SM64_PHYSICS_SCHEDULER_H
#include "objects.h"
/* Original scheduling phases, operating on the active object context. Terrain
 * surfaces/platform movement/collisions must be handled between phases by
 * the eventual complete world scheduler. All bytecode preconditions apply. */
s32 update_objects_in_list(struct ObjectNode *list);
void update_terrain_objects(void);
void update_non_terrain_objects(void);
void unload_deactivated_objects(void);
#endif
