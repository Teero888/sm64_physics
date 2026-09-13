#ifndef SM64_PHYSICS_HOST_ANIMATION_H
#define SM64_PHYSICS_HOST_ANIMATION_H
#include "engine/graph_node.h"
#include "objects.h"

void sm64_objects_set_animation_tick(struct sm64_objects *objects, u16 tick);
u16 sm64_objects_animation_tick(const struct sm64_objects *objects);

/* Simulation owns animation time. Call once after object behaviors for the
 * active object's owning context and its animation tick, regardless of whether a renderer exists.
 * The renderer consumes the resulting frame without advancing it. */
void sm64_physics_advance_object_animation(struct GraphNodeObject *object);
#endif
