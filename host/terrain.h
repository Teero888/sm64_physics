#ifndef SM64_PHYSICS_TERRAIN_H
#define SM64_PHYSICS_TERRAIN_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "engine/surface_collision.h"

struct sm64_terrain;
struct sm64_terrain_triangle {
    int16_t vertices[3][3];
    int16_t type, force;
    int8_t room;
    bool dynamic;
};
struct sm64_terrain_region {
    int16_t kind, low_x, low_z, high_x, high_z, height;
};

/* Input is decoded host data, not a ROM or graphics asset. Creation copies it.
 * The original surface and partition limits are enforced before loading. */
struct sm64_terrain *sm64_terrain_create(const struct sm64_terrain_triangle *triangles,
    size_t count, const struct sm64_terrain_region *regions, size_t region_count);
struct sm64_terrain *sm64_terrain_clone(const struct sm64_terrain *terrain);
void sm64_terrain_destroy(struct sm64_terrain *terrain);

/* Native query functions require an active terrain. Restore the returned
 * previous context after a nested activation. One thread may own a terrain at
 * a time; distinct terrains can be queried concurrently. */
struct sm64_terrain *sm64_terrain_activate(struct sm64_terrain *terrain);
void sm64_terrain_set_query_state(struct sm64_terrain *terrain, bool camera,
    struct Object *current_object, struct Object *mario_object, struct MarioState *mario);
size_t sm64_terrain_surface_count(const struct sm64_terrain *terrain);
#endif
