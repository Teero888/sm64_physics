#ifndef SM64_PHYSICS_HOST_WORLD_H
#define SM64_PHYSICS_HOST_WORLD_H

#include "include/sm64_physics.h"
#include "host/objects.h"
#include "host/terrain.h"
#include "host/audio.h"
#include "host/animation_bank.h"
#include "host/controller.h"
#include "game/mario.h"
#include "game/camera.h"
#include "game/area.h"

struct sm64_world {
    struct sm64_terrain *terrain;
    struct sm64_objects *objects;
    struct sm64_audio_state *audio;
    struct sm64_animation_bank *anim_bank;
    struct MarioState mario;
    struct MarioBodyState body_state;
    struct PlayerCameraState camera_state;
    struct Controller controller;
    struct Area area;
    struct Camera camera;
    struct Object *mario_obj;

    uint32_t frame;
    uint64_t revision;
    bool is_scratch;
    bool failed;
    int16_t level_num;
    float spawn_x, spawn_y, spawn_z;
    int16_t spawn_yaw;

    int16_t cam_yaw;
    int16_t cam_pitch;
    float cam_dist;
};

#endif /* SM64_PHYSICS_HOST_WORLD_H */
