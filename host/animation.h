#ifndef SM64_PHYSICS_HOST_ANIMATION_H
#define SM64_PHYSICS_HOST_ANIMATION_H
#include "engine/graph_node.h"

/* Simulation owns animation time. Call once after object behaviors for the
 * active world's gAreaUpdateCounter, regardless of whether a renderer exists.
 * The renderer consumes the resulting frame without advancing it. */
void sm64_physics_advance_object_animation(struct GraphNodeObject *object);
#endif
