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
    terrain->surfaces = calloc(2300, sizeof(*terrain->surfaces));
    terrain->nodes = calloc(7000, sizeof(*terrain->nodes));
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
    terrain->surface_capacity = 2300;
    terrain->water_floor.type = SURFACE_VERY_SLIPPERY;
    terrain->water_floor.normal.y = 1.0f;
    terrain->environment[0] = (s16) region_count;
    for (size_t i = 0; i < region_count; ++i) {
        TerrainData *out = terrain->environment + 1 + 6 * i;
        out[0] = regions[i].kind; out[1] = regions[i].low_x; out[2] = regions[i].low_z;
        out[3] = regions[i].high_x; out[4] = regions[i].high_z; out[5] = regions[i].height;
    }
    struct sm64_terrain *previous = sm64_terrain_activate(terrain);
    for (int dynamic = 0; dynamic <= 1; ++dynamic) {
      for (size_t i = 0; i < count; ++i) {
        if (triangles[i].dynamic != (bool) dynamic) continue;
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
      if (!dynamic) {
          terrain->static_node_count = terrain->node_count;
          terrain->static_surface_count = terrain->surface_count;
      }
    }
    sm64_terrain_activate(previous);
    return terrain;
}

struct sm64_terrain *sm64_terrain_clone(const struct sm64_terrain *terrain) {
    if (!terrain) return NULL;
    size_t count = (size_t)terrain->surface_count;
    struct sm64_terrain_triangle *triangles = calloc(count ? count : 1, sizeof(*triangles));
    if (!triangles) return NULL;
    for (size_t i = 0; i < count; ++i) {
        const struct Surface *surface = &terrain->surfaces[i];
        memcpy(triangles[i].vertices[0], surface->vertex1, sizeof(surface->vertex1));
        memcpy(triangles[i].vertices[1], surface->vertex2, sizeof(surface->vertex2));
        memcpy(triangles[i].vertices[2], surface->vertex3, sizeof(surface->vertex3));
        triangles[i].type = surface->type;
        triangles[i].force = surface->force;
        triangles[i].room = surface->room;
        triangles[i].dynamic = (surface->flags & SURFACE_FLAG_DYNAMIC) != 0;
    }
    struct sm64_terrain *copy = sm64_terrain_create(triangles, count, terrain->regions, terrain->region_count);
    free(triangles);
    if (!copy) return NULL;
    memcpy(copy->surfaces, terrain->surfaces, count * sizeof(*terrain->surfaces));
    memcpy(copy->environment, terrain->environment, (1 + 6 * terrain->region_count) * sizeof(TerrainData));
    copy->camera = terrain->camera;
    copy->level_num = terrain->level_num;
    copy->camera_movement_flags = terrain->camera_movement_flags;
    copy->water_floor = terrain->water_floor;
    copy->include_intangible = terrain->include_intangible;
    copy->time_stop = terrain->time_stop;
    copy->ddd_warp_behavior = terrain->ddd_warp_behavior;
    copy->current_object = terrain->current_object;
    copy->mario_object = terrain->mario_object;
    copy->mario = terrain->mario;
    return copy;
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

void sm64_terrain_set_time_stop(struct sm64_terrain *terrain, uint32_t flags) {
    terrain->time_stop = flags;
}

void sm64_terrain_set_level(struct sm64_terrain *terrain, int16_t level_num) {
    terrain->level_num = level_num;
}

void sm64_terrain_set_camera_movement_flags(struct sm64_terrain *terrain, int16_t flags) {
    terrain->camera_movement_flags = flags;
}

int16_t sm64_terrain_camera_movement_flags(const struct sm64_terrain *terrain) {
    return terrain->camera_movement_flags;
}

void sm64_terrain_set_ddd_warp_behavior(struct sm64_terrain *terrain, const BehaviorScript *behavior) {
    terrain->ddd_warp_behavior = behavior;
}

bool sm64_terrain_load_object(struct sm64_terrain *terrain, struct Object *object, size_t word_count) {
    if (!terrain || !object || !object->behavior || !object->collisionData || word_count < 3) return false;
    if (object->oDistanceToMario == 19000.0f && !terrain->mario_object) return false;
    const TerrainData *data = object->collisionData;
    if (data[0] != TERRAIN_LOAD_VERTICES || data[1] < 0 || data[1] > 200) return false;
    const size_t vertex_count = (size_t)data[1];
    const size_t groups = 2 + 3 * vertex_count;
    if (groups >= word_count) return false;
    size_t cursor = groups, surfaces = 0;
    while (cursor < word_count && data[cursor] != TERRAIN_LOAD_CONTINUE) {
        if (word_count - cursor < 2) return false;
        const TerrainData type = data[cursor++], count = data[cursor++];
        if (count < 0 || (!TERRAIN_LOAD_IS_SURFACE_TYPE_LOW(type) && !TERRAIN_LOAD_IS_SURFACE_TYPE_HIGH(type))) return false;
        const size_t stride = surface_has_force(type) ? 4 : 3;
        if ((size_t)count > (word_count - cursor) / stride) return false;
        for (size_t i = 0; i < (size_t)count; ++i) {
            for (int j = 0; j < 3; ++j) {
                TerrainData index = data[cursor + i * stride + j];
                if (index < 0 || (size_t)index >= vertex_count) return false;
            }
        }
        surfaces += (size_t)count;
        cursor += (size_t)count * stride;
    }
    if (cursor == word_count || surfaces > (size_t)(2300 - terrain->surface_count)) return false;
    /* The original transform is evaluated on a copy for admission checking.
     * Failed admission must not alter the live object or active query state. */
    struct sm64_terrain *previous = sm64_terrain_activate(terrain);
    struct Object *previous_object = terrain->current_object;
    struct Object probe = *object;
    terrain->current_object = &probe;
    TerrainData transformed[600], *vertex_cursor = (TerrainData *)data + 1;
    transform_object_vertices(&vertex_cursor, transformed);
    terrain->current_object = previous_object;
    sm64_terrain_activate(previous);
    size_t nodes = 0;
    cursor = groups;
    while (data[cursor] != TERRAIN_LOAD_CONTINUE) {
        TerrainData type = data[cursor++];
        size_t count = (size_t)data[cursor++];
        size_t stride = surface_has_force(type) ? 4 : 3;
        for (size_t i = 0; i < count; ++i, cursor += stride) {
            const TerrainData *a = transformed + 3 * data[cursor];
            const TerrainData *b = transformed + 3 * data[cursor + 1];
            const TerrainData *c = transformed + 3 * data[cursor + 2];
            int lx = lower_cell_index(min_3(a[0], b[0], c[0]));
            int hx = upper_cell_index(max_3(a[0], b[0], c[0]));
            int lz = lower_cell_index(min_3(a[2], b[2], c[2]));
            int hz = upper_cell_index(max_3(a[2], b[2], c[2]));
            if (hx >= lx && hz >= lz) nodes += (size_t)(hx - lx + 1) * (hz - lz + 1);
        }
    }
    if (nodes > (size_t)(7000 - terrain->node_count)) return false;
    previous = sm64_terrain_activate(terrain);
    terrain->current_object = object;
    load_object_collision_model();
    terrain->current_object = previous_object;
    sm64_terrain_activate(previous);
    return true;
}
