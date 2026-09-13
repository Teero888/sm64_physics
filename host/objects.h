#ifndef SM64_PHYSICS_OBJECTS_H
#define SM64_PHYSICS_OBJECTS_H
#include "sm64.h"
#include "game/spawn_object.h"
struct sm64_objects;
struct sm64_objects *sm64_objects_create(void);
/* Frees all slots together. Borrowed behavior/assets are not destroyed. */
void sm64_objects_destroy(struct sm64_objects *objects);
/* Returns the previous context. A context may run on only one thread at a
 * time; native deletion calls require objects belonging to the active pool. */
struct sm64_objects *sm64_objects_activate(struct sm64_objects *objects);
void sm64_objects_set_level(struct sm64_objects *objects, s16 level);
/* Requires active objects and valid borrowed behavior bytecode. Returns NULL
 * for invalid list selection or exhaustion with no evictable object. Original
 * create_object retains its infinite-loop exhaustion behavior for native
 * internal callers. Snap-to-floor lists require an active terrain context;
 * eviction/deletion requires an active audio context. */
struct Object *sm64_objects_spawn(const BehaviorScript *behavior);
struct ObjectNode *sm64_objects_list(struct sm64_objects *objects, unsigned list);
#endif
