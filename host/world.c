#include "world.h"
#include "host/objects_state.h"
#include "host/terrain_state.h"
#include "host/camera.h"
#include "host/object_frame.h"
#include "game/object_list_processor.h"
#include "game/interaction.h"

static const BehaviorScript sMarioBehavior[] = {
    /* BEGIN(OBJ_LIST_PLAYER) */
    (((uintptr_t)0x00 << 24) | ((uintptr_t)OBJ_LIST_PLAYER << 16)),
    /* SET_INT(oIntangibleTimer, 0) */
    (((uintptr_t)0x10 << 24) | ((uintptr_t)0x05 << 16)), 0,
    /* OR_INT(oFlags, OBJ_FLAG_0100) */
    (((uintptr_t)0x0F << 24) | ((uintptr_t)0x01 << 16)), ((uintptr_t)OBJ_FLAG_0100 << 16),
    /* OR_INT(oUnk94, 0x0001) */
    (((uintptr_t)0x0F << 24) | ((uintptr_t)0x03 << 16)), ((uintptr_t)0x0001 << 16),
    /* SET_HITBOX(37, 160) */
    (((uintptr_t)0x2F << 24)), (((uintptr_t)37 << 16) | 160),
    /* BEGIN_LOOP() */
    (((uintptr_t)0x08 << 24)),
        /* CALL_NATIVE(bhv_mario_update) */
        (((uintptr_t)0x0C << 24)), (uintptr_t)bhv_mario_update,
    /* END_LOOP() */
    (((uintptr_t)0x09 << 24)),
};

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SM64_STATE_MAGIC 0x34364D53
#define SM64_STATE_VERSION 1

struct sm64_saved_state {
    uint32_t magic;
    uint32_t version;
    uint32_t frame;
    uint64_t revision;
    int16_t cam_yaw, cam_pitch;
    float cam_dist;
    int16_t level_num;
    uint32_t mario_obj_idx;
    struct MarioState mario;
    struct MarioBodyState body;
    struct Controller controller;
    struct sm64_objects objects;
};

struct sm64_checkpoint {
    void *owner;
    uint8_t *data;
    size_t size;
};

static void world_camera_callback(void *user, struct Camera *camera, s16 mode, s16 frames) {
    (void)user; (void)frames;
    if (!camera) return;
    s16 next = (mode == -1) ? camera->defMode : mode;
    camera->mode = next;
}

static inline uintptr_t p_to_rel(const void *base, size_t size, void *p) {
    if (!p) return 0;
    uintptr_t u = (uintptr_t)p;
    uintptr_t start = (uintptr_t)base;
    if (u >= start && u < start + size) {
        return (u - start) + 1;
    }
    return 0;
}

static inline void *rel_to_p(void *base, uintptr_t rel) {
    if (!rel) return NULL;
    return (void *)((uintptr_t)base + (rel - 1));
}

static void objects_to_relative(struct sm64_objects *o) {
    size_t sz = sizeof(*o);
    o->active_lists = (struct ObjectNode *)p_to_rel(o, sz, o->active_lists);
    o->current = (struct Object *)p_to_rel(o, sz, o->current);
    o->mario = (struct Object *)p_to_rel(o, sz, o->mario);
    o->mario_platform = (struct Object *)p_to_rel(o, sz, o->mario_platform);

    o->parent.prev = (struct GraphNode *)p_to_rel(o, sz, o->parent.prev);
    o->parent.next = (struct GraphNode *)p_to_rel(o, sz, o->parent.next);
    o->parent.parent = (struct GraphNode *)p_to_rel(o, sz, o->parent.parent);
    o->parent.children = (struct GraphNode *)p_to_rel(o, sz, o->parent.children);

    o->free_list.next = (struct ObjectNode *)p_to_rel(o, sz, o->free_list.next);
    o->free_list.prev = (struct ObjectNode *)p_to_rel(o, sz, o->free_list.prev);

    for (unsigned l = 0; l < NUM_OBJ_LISTS; ++l) {
        o->lists[l].next = (struct ObjectNode *)p_to_rel(o, sz, o->lists[l].next);
        o->lists[l].prev = (struct ObjectNode *)p_to_rel(o, sz, o->lists[l].prev);
    }

    for (size_t i = 0; i < OBJECT_POOL_CAPACITY; ++i) {
        struct Object *obj = &o->pool[i];
        obj->header.next = (struct ObjectNode *)p_to_rel(o, sz, obj->header.next);
        obj->header.prev = (struct ObjectNode *)p_to_rel(o, sz, obj->header.prev);

        obj->header.gfx.node.prev = (struct GraphNode *)p_to_rel(o, sz, obj->header.gfx.node.prev);
        obj->header.gfx.node.next = (struct GraphNode *)p_to_rel(o, sz, obj->header.gfx.node.next);
        obj->header.gfx.node.parent = (struct GraphNode *)p_to_rel(o, sz, obj->header.gfx.node.parent);
        obj->header.gfx.node.children = (struct GraphNode *)p_to_rel(o, sz, obj->header.gfx.node.children);

        obj->header.gfx.throwMatrix = (Mat4 *)p_to_rel(o, sz, obj->header.gfx.throwMatrix);

        obj->parentObj = (struct Object *)p_to_rel(o, sz, obj->parentObj);
        obj->prevObj = (struct Object *)p_to_rel(o, sz, obj->prevObj);
        obj->platform = (struct Object *)p_to_rel(o, sz, obj->platform);

        for (int c = 0; c < 4; ++c) {
            obj->collidedObjs[c] = (struct Object *)p_to_rel(o, sz, obj->collidedObjs[c]);
        }

#if IS_64_BIT
        for (int p = 0; p < 0x50; ++p) {
            obj->ptrData.asVoidPtr[p] = (void *)p_to_rel(o, sz, obj->ptrData.asVoidPtr[p]);
        }
#endif
    }
}

static void objects_from_relative(struct sm64_objects *o) {
    o->active_lists = (struct ObjectNode *)rel_to_p(o, (uintptr_t)o->active_lists);
    o->current = (struct Object *)rel_to_p(o, (uintptr_t)o->current);
    o->mario = (struct Object *)rel_to_p(o, (uintptr_t)o->mario);
    o->mario_platform = (struct Object *)rel_to_p(o, (uintptr_t)o->mario_platform);

    o->parent.prev = (struct GraphNode *)rel_to_p(o, (uintptr_t)o->parent.prev);
    o->parent.next = (struct GraphNode *)rel_to_p(o, (uintptr_t)o->parent.next);
    o->parent.parent = (struct GraphNode *)rel_to_p(o, (uintptr_t)o->parent.parent);
    o->parent.children = (struct GraphNode *)rel_to_p(o, (uintptr_t)o->parent.children);

    o->free_list.next = (struct ObjectNode *)rel_to_p(o, (uintptr_t)o->free_list.next);
    o->free_list.prev = (struct ObjectNode *)rel_to_p(o, (uintptr_t)o->free_list.prev);

    for (unsigned l = 0; l < NUM_OBJ_LISTS; ++l) {
        o->lists[l].next = (struct ObjectNode *)rel_to_p(o, (uintptr_t)o->lists[l].next);
        o->lists[l].prev = (struct ObjectNode *)rel_to_p(o, (uintptr_t)o->lists[l].prev);
    }

    for (size_t i = 0; i < OBJECT_POOL_CAPACITY; ++i) {
        struct Object *obj = &o->pool[i];
        obj->header.next = (struct ObjectNode *)rel_to_p(o, (uintptr_t)obj->header.next);
        obj->header.prev = (struct ObjectNode *)rel_to_p(o, (uintptr_t)obj->header.prev);

        obj->header.gfx.node.prev = (struct GraphNode *)rel_to_p(o, (uintptr_t)obj->header.gfx.node.prev);
        obj->header.gfx.node.next = (struct GraphNode *)rel_to_p(o, (uintptr_t)obj->header.gfx.node.next);
        obj->header.gfx.node.parent = (struct GraphNode *)rel_to_p(o, (uintptr_t)obj->header.gfx.node.parent);
        obj->header.gfx.node.children = (struct GraphNode *)rel_to_p(o, (uintptr_t)obj->header.gfx.node.children);

        obj->header.gfx.throwMatrix = (Mat4 *)rel_to_p(o, (uintptr_t)obj->header.gfx.throwMatrix);

        obj->parentObj = (struct Object *)rel_to_p(o, (uintptr_t)obj->parentObj);
        obj->prevObj = (struct Object *)rel_to_p(o, (uintptr_t)obj->prevObj);
        obj->platform = (struct Object *)rel_to_p(o, (uintptr_t)obj->platform);

        for (int c = 0; c < 4; ++c) {
            obj->collidedObjs[c] = (struct Object *)rel_to_p(o, (uintptr_t)obj->collidedObjs[c]);
        }

#if IS_64_BIT
        for (int p = 0; p < 0x50; ++p) {
            obj->ptrData.asVoidPtr[p] = rel_to_p(o, (uintptr_t)obj->ptrData.asVoidPtr[p]);
        }
#endif
    }
}

sm64_sim_world *sm64_world_create(
    const sm64_terrain_triangle *triangles, size_t triangle_count,
    const sm64_terrain_region *regions, size_t region_count,
    int16_t level_num,
    float spawn_x, float spawn_y, float spawn_z,
    int16_t spawn_yaw) {
    
    struct sm64_terrain *terrain = NULL;
    if (triangle_count > 2300) {
        size_t max_surfaces = triangle_count + 4096;
        if (max_surfaces < 8192) max_surfaces = 8192;
        size_t max_nodes = triangle_count * 4 + 16384;
        if (max_nodes < 32768) max_nodes = 32768;
        terrain = sm64_terrain_create_extended(
            triangles, triangle_count, regions, region_count, max_surfaces, max_nodes);
    } else {
        terrain = sm64_terrain_create(triangles, triangle_count, regions, region_count);
    }
    if (!terrain) return NULL;

    struct sm64_animation_bank *anim_bank = sm64_animation_bank_default();
    if (!anim_bank) {
        sm64_terrain_destroy(terrain);
        return NULL;
    }

    struct sm64_audio_state *audio = sm64_audio_create(NULL, NULL);
    struct sm64_objects *objects = sm64_objects_create();
    if (!audio || !objects) {
        sm64_terrain_destroy(terrain);
        sm64_animation_bank_destroy(anim_bank);
        if (audio) sm64_audio_destroy(audio);
        if (objects) sm64_objects_destroy(objects);
        return NULL;
    }

    sm64_sim_world *world = calloc(1, sizeof(*world));
    if (!world) {
        sm64_terrain_destroy(terrain);
        sm64_animation_bank_destroy(anim_bank);
        sm64_audio_destroy(audio);
        sm64_objects_destroy(objects);
        return NULL;
    }

    world->terrain = terrain;
    world->anim_bank = anim_bank;
    world->audio = audio;
    world->objects = objects;
    world->level_num = level_num;
    world->spawn_x = spawn_x;
    world->spawn_y = spawn_y;
    world->spawn_z = spawn_z;
    world->spawn_yaw = spawn_yaw;
    world->cam_yaw = spawn_yaw;
    world->cam_pitch = 0x0A00;
    world->cam_dist = 800.0f;
    world->revision = 1;

    sm64_objects_set_level(objects, level_num);
    sm64_terrain_set_level(terrain, level_num);
    sm64_objects_bind_camera(objects, world_camera_callback, world);

    /* Activate contexts to spawn Mario */
    struct sm64_audio_state *prev_audio = sm64_audio_activate(audio);
    struct sm64_terrain *prev_terrain = sm64_terrain_activate(terrain);
    struct sm64_objects *prev_objects = sm64_objects_activate(objects);

    world->mario_obj = sm64_objects_spawn(sMarioBehavior);
    if (!world->mario_obj) {
        sm64_objects_activate(prev_objects);
        sm64_terrain_activate(prev_terrain);
        sm64_audio_activate(prev_audio);
        sm64_world_destroy(world);
        return NULL;
    }
    world->mario_obj->header.gfx.node.flags |= GRAPH_RENDER_ACTIVE;

    world->camera.mode = CAMERA_MODE_CLOSE;
    world->camera.defMode = CAMERA_MODE_CLOSE;
    world->area.camera = &world->camera;
    world->area.terrainType = TERRAIN_GRASS;

    world->mario.marioObj = world->mario_obj;
    world->mario.area = &world->area;
    world->mario.marioBodyState = &world->body_state;
    world->mario.statusForCamera = &world->camera_state;
    world->mario.controller = &world->controller;
    world->mario.animList = sm64_animation_bank_handler(world->anim_bank);
    world->mario.health = 0x880;
    world->mario.numLives = 4;
    world->mario.flags = MARIO_NORMAL_CAP | MARIO_CAP_ON_HEAD;
    world->mario.pos[0] = spawn_x;
    world->mario.pos[1] = spawn_y;
    world->mario.pos[2] = spawn_z;
    world->mario.faceAngle[1] = spawn_yaw;

    world->mario.floorHeight = find_floor(spawn_x, spawn_y + 100.0f, spawn_z, &world->mario.floor);
    if (world->mario.floor && spawn_y < world->mario.floorHeight) {
        world->mario.pos[1] = world->mario.floorHeight;
    }
    world->mario.action = (world->mario.floor ? ACT_IDLE : ACT_FREEFALL);
    set_mario_animation(&world->mario, 0);

    world->mario_obj->header.gfx.pos[0] = world->mario.pos[0];
    world->mario_obj->header.gfx.pos[1] = world->mario.pos[1];
    world->mario_obj->header.gfx.pos[2] = world->mario.pos[2];
    world->mario_obj->header.gfx.angle[1] = spawn_yaw;

    sm64_objects_set_mario(world->objects, world->mario_obj);
    sm64_objects_set_motion_state(world->objects, &world->mario, NULL, 0);
    sm64_terrain_set_query_state(world->terrain, false, NULL, world->mario_obj, &world->mario);

    sm64_objects_activate(prev_objects);
    sm64_terrain_activate(prev_terrain);
    sm64_audio_activate(prev_audio);

    return world;
}

void sm64_world_destroy(sm64_sim_world *world) {
    if (!world) return;
    if (world->objects) sm64_objects_destroy(world->objects);
    if (world->terrain) sm64_terrain_destroy(world->terrain);
    if (world->anim_bank) sm64_animation_bank_destroy(world->anim_bank);
    if (world->audio) sm64_audio_destroy(world->audio);
    free(world);
}

void sm64_world_set_scratch(sm64_sim_world *world, bool scratch) {
    if (world) world->is_scratch = scratch;
}

void sm64_world_copy(sm64_sim_world *dst, const sm64_sim_world *src) {
    if (!dst || !src || dst == src) return;

    dst->frame = src->frame;
    dst->revision = src->revision;
    dst->is_scratch = src->is_scratch;
    dst->failed = src->failed;
    dst->level_num = src->level_num;
    dst->spawn_x = src->spawn_x;
    dst->spawn_y = src->spawn_y;
    dst->spawn_z = src->spawn_z;
    dst->spawn_yaw = src->spawn_yaw;
    dst->cam_yaw = src->cam_yaw;
    dst->cam_pitch = src->cam_pitch;
    dst->cam_dist = src->cam_dist;

    dst->body_state = src->body_state;
    dst->camera_state = src->camera_state;
    dst->controller = src->controller;
    dst->area = src->area;
    dst->camera = src->camera;
    dst->mario = src->mario;

    if (dst->terrain) sm64_terrain_destroy(dst->terrain);
    dst->terrain = sm64_terrain_clone(src->terrain);

    if (dst->anim_bank) sm64_animation_bank_destroy(dst->anim_bank);
    dst->anim_bank = sm64_animation_bank_clone(src->anim_bank);

    if (!dst->audio) dst->audio = sm64_audio_create(NULL, NULL);
    if (!dst->objects) dst->objects = sm64_objects_create();
    sm64_objects_copy(dst->objects, src->objects);

    /* Rebind pointers */
    ptrdiff_t mario_idx = src->mario_obj ? (src->mario_obj - src->objects->pool) : 0;
    if (mario_idx >= 0 && mario_idx < OBJECT_POOL_CAPACITY) {
        dst->mario_obj = &dst->objects->pool[mario_idx];
    } else {
        dst->mario_obj = NULL;
    }

    dst->area.camera = &dst->camera;
    dst->mario.marioObj = dst->mario_obj;
    dst->mario.area = &dst->area;
    dst->mario.marioBodyState = &dst->body_state;
    dst->mario.statusForCamera = &dst->camera_state;
    dst->mario.controller = &dst->controller;
    dst->mario.animList = sm64_animation_bank_handler(dst->anim_bank);

    sm64_objects_bind_camera(dst->objects, world_camera_callback, dst);
    sm64_objects_set_mario(dst->objects, dst->mario_obj);
    sm64_objects_set_motion_state(dst->objects, &dst->mario, dst->objects->current, dst->objects->time_stop);
    sm64_terrain_set_query_state(dst->terrain, false, dst->objects->current, dst->mario_obj, &dst->mario);
}

sm64_sim_world *sm64_world_clone(const sm64_sim_world *src) {
    if (!src) return NULL;
    sm64_sim_world *dst = calloc(1, sizeof(*dst));
    if (!dst) return NULL;
    sm64_world_copy(dst, src);
    return dst;
}

bool sm64_world_step(sm64_sim_world *world, sm64_input input, char *error, size_t error_size) {
    if (!world) {
        if (error && error_size) snprintf(error, error_size, "Missing SM64 world");
        return false;
    }
    if (world->failed) return false;

    sm64_controller_update(&world->controller, input.buttons, input.stick_x, input.stick_y);
    world->mario.input = 0;
    update_mario_button_inputs(&world->mario);
    update_mario_joystick_inputs(&world->mario);

    if (input.buttons & 0x0200) world->cam_yaw += 0x0800;
    if (input.buttons & 0x0100) world->cam_yaw -= 0x0800;
    if (input.buttons & 0x0800) {
        if (world->cam_dist > 400.0f) world->cam_dist -= 50.0f;
    }
    if (input.buttons & 0x0400) {
        if (world->cam_dist < 2000.0f) world->cam_dist += 50.0f;
    }

    struct sm64_audio_state *prev_audio = sm64_audio_activate(world->audio);
    struct sm64_terrain *prev_terrain = sm64_terrain_activate(world->terrain);
    struct sm64_objects *prev_objects = sm64_objects_activate(world->objects);

    gCurrentArea = &world->area;
    gCurrAreaIndex = 1;
    gCurrLevelNum = world->level_num;

    gMarioStates[0] = world->mario;
    gMarioStates[0].marioObj = world->mario_obj;
    gMarioStates[0].area = &world->area;
    gMarioStates[0].marioBodyState = &world->body_state;
    gMarioStates[0].statusForCamera = &world->camera_state;
    gMarioStates[0].controller = &world->controller;
    gMarioStates[0].animList = sm64_animation_bank_handler(world->anim_bank);
    gMarioState = &gMarioStates[0];

    sm64_objects_set_mario(world->objects, world->mario_obj);
    sm64_objects_set_motion_state(world->objects, gMarioState, NULL, world->objects->time_stop);
    sm64_terrain_set_query_state(world->terrain, false, world->objects->current, world->mario_obj, gMarioState);

    sm64_objects_step(world->objects, world->terrain, world->frame);

    world->mario = gMarioStates[0];

    ++world->frame;
    ++world->revision;

    sm64_objects_activate(prev_objects);
    sm64_terrain_activate(prev_terrain);
    sm64_audio_activate(prev_audio);
    return true;
}

uint32_t sm64_sim_world_view(const sm64_sim_world *world, sm64_view *out) {
    if (!world || !out) return 0;
    out->pos[0] = world->mario.pos[0];
    out->pos[1] = world->mario.pos[1];
    out->pos[2] = world->mario.pos[2];
    out->vel[0] = world->mario.vel[0];
    out->vel[1] = world->mario.vel[1];
    out->vel[2] = world->mario.vel[2];
    out->action = world->mario.action;
    out->health = (int32_t)world->mario.health;
    out->frame = world->frame;
    out->valid = !world->failed && world->mario_obj != NULL;
    return world->frame;
}

bool sm64_world_pose(const sm64_sim_world *world, sm64_scene_mario *out) {
    if (!world || !out) return false;
    out->pos[0] = world->mario.pos[0];
    out->pos[1] = world->mario.pos[1];
    out->pos[2] = world->mario.pos[2];
    if (world->mario_obj) {
        out->scale[0] = world->mario_obj->header.gfx.scale[0];
        out->scale[1] = world->mario_obj->header.gfx.scale[1];
        out->scale[2] = world->mario_obj->header.gfx.scale[2];
        out->angle[0] = world->mario_obj->header.gfx.angle[0];
        out->angle[1] = world->mario_obj->header.gfx.angle[1];
        out->angle[2] = world->mario_obj->header.gfx.angle[2];
        out->anim_id = world->mario_obj->header.gfx.animInfo.animID;
        out->anim_frame = world->mario_obj->header.gfx.animInfo.animFrame;
        out->anim_accel = world->mario_obj->header.gfx.animInfo.animAccel;
    } else {
        out->scale[0] = out->scale[1] = out->scale[2] = 1.0f;
        out->angle[0] = out->angle[1] = out->angle[2] = 0;
        out->anim_id = 0;
        out->anim_frame = 0;
        out->anim_accel = 0;
    }
    out->area = 1;
    out->valid = !world->failed && world->mario_obj != NULL;
    return out->valid;
}

static void vec3_cross(const float a[3], const float b[3], float out[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

static float vec3_dot(const float a[3], const float b[3]) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static bool vec3_normalize(float v[3]) {
    float len = sqrtf(vec3_dot(v, v));
    if (len < 1e-6f) return false;
    float inv = 1.0f / len;
    v[0] *= inv; v[1] *= inv; v[2] *= inv;
    return true;
}

static void mat4_multiply(float out[16], const float a[16], const float b[16]) {
    float res[16];
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            res[col * 4 + row] =
                a[0 * 4 + row] * b[col * 4 + 0] +
                a[1 * 4 + row] * b[col * 4 + 1] +
                a[2 * 4 + row] * b[col * 4 + 2] +
                a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
    memcpy(out, res, sizeof(res));
}

bool sm64_world_camera(const sm64_sim_world *world, float aspect, sm64_camera *out) {
    if (!world || !out || aspect <= 0.0f || !isfinite(aspect)) return false;

    float target[3] = {
        world->mario.pos[0],
        world->mario.pos[1] + 120.0f,
        world->mario.pos[2]
    };

    float yaw_rad = (float)world->cam_yaw * (float)(M_PI / 32768.0);
    float pitch_rad = (float)world->cam_pitch * (float)(M_PI / 32768.0);
    float h_dist = world->cam_dist * cosf(pitch_rad);

    float eye[3] = {
        target[0] - sinf(yaw_rad) * h_dist,
        target[1] + world->cam_dist * sinf(pitch_rad),
        target[2] - cosf(yaw_rad) * h_dist
    };

    float up[3] = {0.0f, 1.0f, 0.0f};

    out->eye[0] = eye[0]; out->eye[1] = eye[1]; out->eye[2] = eye[2];
    out->target[0] = target[0]; out->target[1] = target[1]; out->target[2] = target[2];
    out->up[0] = up[0]; out->up[1] = up[1]; out->up[2] = up[2];
    out->fov_y = 45.0f * (float)(M_PI / 180.0);
    out->near_z = 100.0f;
    out->far_z = 20000.0f;

    /* View matrix */
    float f[3] = { target[0] - eye[0], target[1] - eye[1], target[2] - eye[2] };
    vec3_normalize(f);
    float u[3] = { up[0], up[1], up[2] };
    vec3_normalize(u);
    float s[3];
    vec3_cross(f, u, s);
    vec3_normalize(s);
    vec3_cross(s, f, u);

    float view_m[16] = {0};
    view_m[0] = s[0];  view_m[4] = s[1];  view_m[8]  = s[2];  view_m[12] = -vec3_dot(s, eye);
    view_m[1] = u[0];  view_m[5] = u[1];  view_m[9]  = u[2];  view_m[13] = -vec3_dot(u, eye);
    view_m[2] = -f[0]; view_m[6] = -f[1]; view_m[10] = -f[2]; view_m[14] = vec3_dot(f, eye);
    view_m[15] = 1.0f;

    /* Perspective matrix */
    float tan_half_fovy = tanf(out->fov_y / 2.0f);
    float proj_m[16] = {0};
    proj_m[0] = 1.0f / (aspect * tan_half_fovy);
    proj_m[5] = 1.0f / tan_half_fovy;
    proj_m[10] = -(out->far_z + out->near_z) / (out->far_z - out->near_z);
    proj_m[11] = -1.0f;
    proj_m[14] = -(2.0f * out->far_z * out->near_z) / (out->far_z - out->near_z);

    mat4_multiply(out->view_proj, proj_m, view_m);
    for (int i = 0; i < 16; ++i) {
        if (!isfinite(out->view_proj[i])) return false;
    }
    return true;
}

uint64_t sm64_world_revision(const sm64_sim_world *world) {
    return world ? world->revision : 0;
}

void *sm64_world_mario_state(sm64_sim_world *world) {
    return world ? &world->mario : NULL;
}

bool sm64_world_set(sm64_sim_world *world, uint32_t property, const sm64_view *value, char *error, size_t error_size) {
    if (!world || !value) {
        if (error && error_size) snprintf(error, error_size, "Missing SM64 state edit");
        return false;
    }
    if (property == 0) {
        if (!isfinite(value->pos[0]) || !isfinite(value->pos[1]) || !isfinite(value->pos[2])) {
            if (error && error_size) snprintf(error, error_size, "Position values must be finite");
            return false;
        }
        world->mario.pos[0] = value->pos[0];
        world->mario.pos[1] = value->pos[1];
        world->mario.pos[2] = value->pos[2];
        if (world->mario_obj) {
            world->mario_obj->header.gfx.pos[0] = value->pos[0];
            world->mario_obj->header.gfx.pos[1] = value->pos[1];
            world->mario_obj->header.gfx.pos[2] = value->pos[2];
            world->mario_obj->oPosX = value->pos[0];
            world->mario_obj->oPosY = value->pos[1];
            world->mario_obj->oPosZ = value->pos[2];
        }
    } else if (property == 1) {
        if (!isfinite(value->vel[0]) || !isfinite(value->vel[1]) || !isfinite(value->vel[2])) {
            if (error && error_size) snprintf(error, error_size, "Velocity values must be finite");
            return false;
        }
        world->mario.vel[0] = value->vel[0];
        world->mario.vel[1] = value->vel[1];
        world->mario.vel[2] = value->vel[2];
        if (world->mario_obj) {
            world->mario_obj->oVelX = value->vel[0];
            world->mario_obj->oVelY = value->vel[1];
            world->mario_obj->oVelZ = value->vel[2];
        }
    } else if (property == 2) {
        if (error && error_size) snprintf(error, error_size, "Action property is read-only");
        return false;
    } else if (property == 3) {
        if (value->health < 0 || value->health > SM64_MARIO_MAX_HEALTH) {
            if (error && error_size) snprintf(error, error_size, "Invalid SM64 health value");
            return false;
        }
        world->mario.health = (s16)value->health;
    } else {
        if (error && error_size) snprintf(error, error_size, "Invalid SM64 property index");
        return false;
    }
    ++world->revision;
    return true;
}

size_t sm64_world_save(const sm64_sim_world *world, uint8_t *out, size_t size, char *error, size_t error_size) {
    if (!world) {
        if (error && error_size) snprintf(error, error_size, "Missing SM64 world");
        return 0;
    }
    size_t needed = sizeof(struct sm64_saved_state);
    if (!out) return needed;
    if (size < needed) {
        if (error && error_size) snprintf(error, error_size, "State buffer is too small");
        return 0;
    }

    struct sm64_saved_state saved = {0};
    saved.magic = SM64_STATE_MAGIC;
    saved.version = SM64_STATE_VERSION;
    saved.frame = world->frame;
    saved.revision = world->revision;
    saved.cam_yaw = world->cam_yaw;
    saved.cam_pitch = world->cam_pitch;
    saved.cam_dist = world->cam_dist;
    saved.level_num = world->level_num;

    saved.mario_obj_idx = world->mario_obj ? (uint32_t)(world->mario_obj - world->objects->pool) : 0;
    saved.mario = world->mario;
    saved.mario.marioObj = NULL;
    saved.mario.area = NULL;
    saved.mario.marioBodyState = NULL;
    saved.mario.statusForCamera = NULL;
    saved.mario.controller = NULL;
    saved.mario.animList = NULL;
    saved.body = world->body_state;
    saved.controller = world->controller;

    sm64_objects_copy(&saved.objects, world->objects);
    objects_to_relative(&saved.objects);

    memcpy(out, &saved, sizeof(saved));
    return needed;
}

bool sm64_world_load(sm64_sim_world *world, const uint8_t *data, size_t size, char *error, size_t error_size) {
    if (!world || !data) {
        if (error && error_size) snprintf(error, error_size, "Missing SM64 world or data");
        return false;
    }
    if (size < sizeof(struct sm64_saved_state)) {
        if (error && error_size) snprintf(error, error_size, "State buffer size invalid");
        return false;
    }

    struct sm64_saved_state saved;
    memcpy(&saved, data, sizeof(saved));
    if (saved.magic != SM64_STATE_MAGIC || saved.version != SM64_STATE_VERSION) {
        if (error && error_size) snprintf(error, error_size, "Invalid SM64 state header");
        return false;
    }

    world->frame = saved.frame;
    world->revision = saved.revision;
    world->cam_yaw = saved.cam_yaw;
    world->cam_pitch = saved.cam_pitch;
    world->cam_dist = saved.cam_dist;
    world->level_num = saved.level_num;
    world->body_state = saved.body;
    world->controller = saved.controller;
    world->mario = saved.mario;

    objects_from_relative(&saved.objects);
    sm64_objects_copy(world->objects, &saved.objects);

    if (saved.mario_obj_idx < OBJECT_POOL_CAPACITY) {
        world->mario_obj = &world->objects->pool[saved.mario_obj_idx];
    } else {
        world->mario_obj = NULL;
    }

    world->area.camera = &world->camera;
    world->mario.marioObj = world->mario_obj;
    world->mario.area = &world->area;
    world->mario.marioBodyState = &world->body_state;
    world->mario.statusForCamera = &world->camera_state;
    world->mario.controller = &world->controller;
    world->mario.animList = sm64_animation_bank_handler(world->anim_bank);

    sm64_objects_bind_camera(world->objects, world_camera_callback, world);
    sm64_objects_set_mario(world->objects, world->mario_obj);
    sm64_objects_set_motion_state(world->objects, &world->mario, world->objects->current, world->objects->time_stop);
    sm64_terrain_set_query_state(world->terrain, false, world->objects->current, world->mario_obj, &world->mario);

    world->failed = false;
    return true;
}

/* Worker physics callbacks */

#define MAX_PHYSICS_WORKERS 64

struct sm64_physics_worker {
    sm64_physics api;
    sm64_sim_world *world;
    sm64_view view;
    int slot;
};

static struct sm64_physics_worker *g_workers[MAX_PHYSICS_WORKERS];

static void worker_do_step(int slot, sm64_input input) {
    if (slot < 0 || slot >= MAX_PHYSICS_WORKERS) return;
    struct sm64_physics_worker *w = g_workers[slot];
    if (!w || !w->world) return;
    sm64_world_step(w->world, input, NULL, 0);
    sm64_sim_world_view(w->world, &w->view);
    w->api.mario = &w->world->mario;
}

#define T(n) static void worker_step_##n(sm64_input in) { worker_do_step(n, in); }
T(0)  T(1)  T(2)  T(3)  T(4)  T(5)  T(6)  T(7)  T(8)  T(9)  T(10) T(11) T(12) T(13) T(14) T(15)
T(16) T(17) T(18) T(19) T(20) T(21) T(22) T(23) T(24) T(25) T(26) T(27) T(28) T(29) T(30) T(31)
T(32) T(33) T(34) T(35) T(36) T(37) T(38) T(39) T(40) T(41) T(42) T(43) T(44) T(45) T(46) T(47)
T(48) T(49) T(50) T(51) T(52) T(53) T(54) T(55) T(56) T(57) T(58) T(59) T(60) T(61) T(62) T(63)
#undef T

typedef void (*worker_step_fn)(sm64_input);
#define P(n) worker_step_##n,
static const worker_step_fn g_step_thunks[MAX_PHYSICS_WORKERS] = {
    P(0)  P(1)  P(2)  P(3)  P(4)  P(5)  P(6)  P(7)  P(8)  P(9)  P(10) P(11) P(12) P(13) P(14) P(15)
    P(16) P(17) P(18) P(19) P(20) P(21) P(22) P(23) P(24) P(25) P(26) P(27) P(28) P(29) P(30) P(31)
    P(32) P(33) P(34) P(35) P(36) P(37) P(38) P(39) P(40) P(41) P(42) P(43) P(44) P(45) P(46) P(47)
    P(48) P(49) P(50) P(51) P(52) P(53) P(54) P(55) P(56) P(57) P(58) P(59) P(60) P(61) P(62) P(63)
};
#undef P

static sm64_checkpoint *worker_capture(sm64_physics *physics, char *error, size_t error_size) {
    if (!physics) return NULL;
    struct sm64_physics_worker *w = (struct sm64_physics_worker *)physics->owner;
    if (!w || !w->world) {
        if (error && error_size) snprintf(error, error_size, "Missing physics worker");
        return NULL;
    }
    size_t sz = sm64_world_save(w->world, NULL, 0, NULL, 0);
    if (!sz) {
        if (error && error_size) snprintf(error, error_size, "Failed to calculate save size");
        return NULL;
    }
    sm64_checkpoint *cp = calloc(1, sizeof(*cp));
    if (!cp) return NULL;
    cp->data = malloc(sz);
    if (!cp->data) { free(cp); return NULL; }
    cp->size = sz;
    cp->owner = w;
    sm64_world_save(w->world, cp->data, cp->size, error, error_size);
    return cp;
}

static bool worker_restore(sm64_physics *physics, const sm64_checkpoint *cp, char *error, size_t error_size) {
    if (!physics || !cp) {
        if (error && error_size) snprintf(error, error_size, "Missing physics worker or checkpoint");
        return false;
    }
    struct sm64_physics_worker *w = (struct sm64_physics_worker *)physics->owner;
    if (!w || cp->owner != w) {
        if (error && error_size) snprintf(error, error_size, "Foreign checkpoint rejected");
        return false;
    }
    bool ok = sm64_world_load(w->world, cp->data, cp->size, error, error_size);
    if (ok) {
        sm64_sim_world_view(w->world, &w->view);
        w->api.mario = &w->world->mario;
    }
    return ok;
}

static void worker_free_checkpoint(sm64_checkpoint *cp) {
    if (!cp) return;
    free(cp->data);
    free(cp);
}

static void worker_destroy(sm64_physics *physics) {
    if (!physics) return;
    struct sm64_physics_worker *w = (struct sm64_physics_worker *)physics->owner;
    if (!w) return;
    if (w->slot >= 0 && w->slot < MAX_PHYSICS_WORKERS) {
        g_workers[w->slot] = NULL;
    }
    if (w->world) sm64_world_destroy(w->world);
    free(w);
}

sm64_physics *sm64_world_physics_create(const sm64_sim_world *world, char *error, size_t error_size) {
    if (!world) {
        if (error && error_size) snprintf(error, error_size, "Missing source world");
        return NULL;
    }
    int slot = -1;
    for (int i = 0; i < MAX_PHYSICS_WORKERS; ++i) {
        if (!g_workers[i]) { slot = i; break; }
    }
    if (slot < 0) {
        if (error && error_size) snprintf(error, error_size, "Maximum physics workers reached");
        return NULL;
    }
    struct sm64_physics_worker *w = calloc(1, sizeof(*w));
    if (!w) return NULL;
    w->slot = slot;
    w->world = sm64_world_clone(world);
    if (!w->world) { free(w); return NULL; }

    w->api.step = g_step_thunks[slot];
    w->api.view = &w->view;
    w->api.mario = &w->world->mario;
    w->api.capture = worker_capture;
    w->api.restore = worker_restore;
    w->api.free_checkpoint = worker_free_checkpoint;
    w->api.destroy = worker_destroy;
    w->api.owner = w;

    g_workers[slot] = w;
    sm64_sim_world_view(w->world, &w->view);
    return &w->api;
}
