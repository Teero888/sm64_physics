/* Included after the verbatim loader helpers, which remain file-local. */
#include <stdlib.h>
#include <string.h>

_Thread_local struct sm64_terrain *sm64_active_terrain;

struct sm64_terrain *sm64_terrain_activate(struct sm64_terrain *terrain) {
    struct sm64_terrain *previous = sm64_active_terrain;
    sm64_active_terrain = terrain;
    return previous;
}

void sm64_terrain_destroy(struct sm64_terrain *terrain) {
    if (terrain == NULL) return;
    if (sm64_active_terrain == terrain) sm64_active_terrain = NULL;
    free(terrain->nodes);
    free(terrain->surfaces);
    free(terrain->triangles);
    free(terrain->environment);
    free(terrain->regions);
    free(terrain);
}

struct sm64_terrain *sm64_terrain_create(const struct sm64_terrain_triangle *triangles,
    size_t count, const struct sm64_terrain_region *regions, size_t region_count) {
    if (count > 2300 || region_count > 20 || (count && !triangles) || (region_count && !regions)) return NULL;
    size_t nodes = 0;
    for (size_t i = 0; i < count; ++i) {
        const int16_t (*v)[3] = triangles[i].vertices;
        int low_x = lower_cell_index(min_3(v[0][0], v[1][0], v[2][0]));
        int high_x = upper_cell_index(max_3(v[0][0], v[1][0], v[2][0]));
        int low_z = lower_cell_index(min_3(v[0][2], v[1][2], v[2][2]));
        int high_z = upper_cell_index(max_3(v[0][2], v[1][2], v[2][2]));
        if (high_x >= low_x && high_z >= low_z) nodes += (size_t)(high_x - low_x + 1) * (high_z - low_z + 1);
    }
    if (nodes > 7000) return NULL;
    struct sm64_terrain *terrain = calloc(1, sizeof(*terrain));
    if (!terrain) return NULL;
    terrain->surfaces = calloc(count ? count : 1, sizeof(*terrain->surfaces));
    terrain->nodes = calloc(nodes ? nodes : 1, sizeof(*terrain->nodes));
    terrain->triangles = calloc(count ? count : 1, sizeof(*triangles));
    terrain->regions = calloc(region_count ? region_count : 1, sizeof(*regions));
    terrain->environment = calloc(1 + 6 * region_count, sizeof(TerrainData));
    if (!terrain->surfaces || !terrain->nodes || !terrain->triangles || !terrain->regions || !terrain->environment) {
        sm64_terrain_destroy(terrain);
        return NULL;
    }
    if (count) memcpy(terrain->triangles, triangles, count * sizeof(*triangles));
    if (region_count) memcpy(terrain->regions, regions, region_count * sizeof(*regions));
    terrain->triangle_count = count;
    terrain->region_count = region_count;
    terrain->surface_capacity = (s16) count;
    terrain->environment[0] = (s16) region_count;
    for (size_t i = 0; i < region_count; ++i) {
        TerrainData *out = terrain->environment + 1 + 6 * i;
        out[0] = regions[i].kind; out[1] = regions[i].low_x; out[2] = regions[i].low_z;
        out[3] = regions[i].high_x; out[4] = regions[i].high_z; out[5] = regions[i].height;
    }
    struct sm64_terrain *previous = sm64_terrain_activate(terrain);
    for (size_t i = 0; i < count; ++i) {
        TerrainData vertices[9];
        memcpy(vertices, triangles[i].vertices, sizeof(vertices));
        TerrainData indices[3] = {0, 1, 2}, *cursor = indices;
        struct Surface *surface = read_surface_data(vertices, &cursor);
        if (!surface) continue;
        surface->type = triangles[i].type;
        surface->force = surface_has_force(surface->type) ? triangles[i].force : 0;
        surface->room = triangles[i].room;
        surface->flags = (s8) surf_has_no_cam_collision(surface->type);
        if (triangles[i].dynamic) surface->flags |= SURFACE_FLAG_DYNAMIC;
        add_surface(surface, triangles[i].dynamic);
    }
    sm64_terrain_activate(previous);
    return terrain;
}

struct sm64_terrain *sm64_terrain_clone(const struct sm64_terrain *terrain) {
    if (!terrain) return NULL;
    return sm64_terrain_create(terrain->triangles, terrain->triangle_count,
                              terrain->regions, terrain->region_count);
}

void sm64_terrain_set_query_state(struct sm64_terrain *terrain, bool camera,
    struct Object *current_object, struct Object *mario_object, struct MarioState *mario) {
    terrain->camera = camera;
    terrain->current_object = current_object;
    terrain->mario_object = mario_object;
    terrain->mario = mario;
}

size_t sm64_terrain_surface_count(const struct sm64_terrain *terrain) {
    return terrain ? (size_t) terrain->surface_count : 0;
}
