#ifndef SM64_PHYSICS_H
#define SM64_PHYSICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SM64_PHYSICS_TYPES_DEFINED
#define SM64_PHYSICS_TYPES_DEFINED

/* SM64 runs at 30 ticks per second. Max health is 0x880 (8 wedges of 0x100). */
enum {
    SM64_TICKS_PER_SECOND = 30,
    SM64_MARIO_MAX_HEALTH = 0x880,
};

typedef struct sm64_input {
    uint16_t buttons;
    int8_t stick_x, stick_y;
} sm64_input;

typedef struct sm64_view {
    float pos[3], vel[3];
    uint32_t action;
    int32_t health;
    uint32_t frame;
    bool valid;
} sm64_view;

typedef struct sm64_scene_mario {
    float pos[3], scale[3];
    int16_t angle[3], anim_id, anim_frame;
    int32_t anim_accel;
    uint8_t area;
    bool valid;
} sm64_scene_mario;

typedef struct sm64_camera {
    float eye[3];
    float target[3];
    float up[3];
    float view_proj[16];
    float fov_y;
    float near_z;
    float far_z;
} sm64_camera;

typedef struct sm64_physics sm64_physics;
typedef struct sm64_checkpoint sm64_checkpoint;

/* Each isolated physics instance owns a separate native image / world.
 * One thread owns an instance; different instances can step concurrently.
 * step updates the live view without any graphics or locks. */
struct sm64_physics {
    void (*step)(sm64_input input);
    const sm64_view *view;
    void *mario;
    sm64_checkpoint *(*capture)(sm64_physics *physics, char *error, size_t error_size);
    bool (*restore)(sm64_physics *physics, const sm64_checkpoint *checkpoint, char *error, size_t error_size);
    void (*free_checkpoint)(sm64_checkpoint *checkpoint);
    void (*destroy)(sm64_physics *physics);
    void *owner;
};

#endif /* SM64_PHYSICS_TYPES_DEFINED */

#ifndef SM64_TERRAIN_TRIANGLE_DECLARED
#define SM64_TERRAIN_TRIANGLE_DECLARED
struct sm64_terrain_triangle {
    int16_t vertices[3][3];
    int16_t type, force;
    int8_t room;
    bool dynamic;
};
typedef struct sm64_terrain_triangle sm64_terrain_triangle;

struct sm64_terrain_region {
    int16_t kind, low_x, low_z, high_x, high_z, height;
};
typedef struct sm64_terrain_region sm64_terrain_region;
#endif

typedef struct sm64_sim_world sm64_sim_world;

/* World lifecycle */
sm64_sim_world *sm64_world_create(
    const sm64_terrain_triangle *triangles, size_t triangle_count,
    const sm64_terrain_region *regions, size_t region_count,
    int16_t level_num,
    float spawn_x, float spawn_y, float spawn_z,
    int16_t spawn_yaw);

void sm64_world_destroy(sm64_sim_world *world);
sm64_sim_world *sm64_world_clone(const sm64_sim_world *src);
void sm64_world_copy(sm64_sim_world *dst, const sm64_sim_world *src);
void sm64_world_set_scratch(sm64_sim_world *world, bool scratch);

/* Simulation stepping */
bool sm64_world_step(sm64_sim_world *world, sm64_input input, char *error, size_t error_size);

/* Queries */
uint32_t sm64_sim_world_view(const sm64_sim_world *world, sm64_view *out);
bool sm64_world_pose(const sm64_sim_world *world, sm64_scene_mario *out);
bool sm64_world_camera(const sm64_sim_world *world, float aspect, sm64_camera *out);
uint64_t sm64_world_revision(const sm64_sim_world *world);
void *sm64_world_mario_state(sm64_sim_world *world);

/* Property editing: 0 = pos, 1 = vel, 2 = action (read-only), 3 = health */
bool sm64_world_set(sm64_sim_world *world, uint32_t property, const sm64_view *value, char *error, size_t error_size);
uint32_t sm64_world_edit_count(const sm64_sim_world *world);

/* State serialization */
size_t sm64_world_save(const sm64_sim_world *world, uint8_t *out, size_t size, char *error, size_t error_size);
bool sm64_world_load(sm64_sim_world *world, const uint8_t *data, size_t size, char *error, size_t error_size);

/* Isolated physics worker for predictions / TAS plugins */
sm64_physics *sm64_world_physics_create(const sm64_sim_world *world, char *error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif /* SM64_PHYSICS_H */
