#include <assert.h>
#include <stdio.h>
#include "../host/object_frame.h"
#include "../host/audio.h"
#include "../host/animation.h"
#include "engine/surface_load.h"
#include "game/object_list_processor.h"
#include "game/interaction.h"
static struct sm64_objects *world;
static struct Object *platform, *player;
static struct MarioState mario;
static int surface_calls, player_calls, tail_calls, request_stop;
static float observed_x;
static void move_surface(void) {
    platform->oPosX += 5;
    platform->oVelX = 5;
    ++surface_calls;
    load_object_collision_model();
}
static void observe_player(void) {
    ++player_calls;
    observed_x = mario.pos[0];
    player->oPosX = mario.pos[0];
    assert(player->collidedObjInteractTypes & INTERACT_COIN);
}
static void tail(void) {
    ++tail_calls;
    if (request_stop) sm64_objects_set_time_stop(world, TIME_STOP_ENABLED);
}
#define SCRIPT(list, fn) {(list)<<16, 0x08000000, 0x0c000000, (uintptr_t)(fn), 0x09000000}
static const BehaviorScript surface[] = SCRIPT(OBJ_LIST_SURFACE, move_surface);
static const BehaviorScript player_script[] = SCRIPT(OBJ_LIST_PLAYER, observe_player);
static const BehaviorScript tail_script[] = SCRIPT(OBJ_LIST_DEFAULT, tail);
static const BehaviorScript coin_script[] = {OBJ_LIST_LEVEL << 16, 0x0a000000};
int main(void) {
    world = sm64_objects_create();
    struct sm64_terrain *terrain = sm64_terrain_create(NULL, 0, NULL, 0);
    struct sm64_audio_state *audio = sm64_audio_create(NULL, NULL);
    assert(world && terrain && audio);
    sm64_audio_activate(audio);
    sm64_objects_activate(world);
    platform = sm64_objects_spawn(surface);
    player = sm64_objects_spawn(player_script);
    struct Object *coin = sm64_objects_spawn(coin_script);
    assert(sm64_objects_spawn(tail_script));
    struct Animation animation = {.loopEnd = 100};
    struct Animation *animation_pointer = &animation;
    geo_obj_init_animation(&coin->header.gfx, &animation_pointer);
    coin->header.gfx.node.flags |= GRAPH_RENDER_ACTIVE;
    TerrainData collision[] = {TERRAIN_LOAD_VERTICES, 3, -100,0,-100, 0,0,100, 100,0,-100,
        SURFACE_DEFAULT, 1, 0,1,2, TERRAIN_LOAD_CONTINUE};
    platform->collisionData = collision;
    platform->oDistanceToMario = 0;
    player->oIntangibleTimer = coin->oIntangibleTimer = 0;
    coin->oInteractType = INTERACT_COIN;
    sm64_objects_set_mario(world, player);
    sm64_objects_set_motion_state(world, &mario, NULL, 0);
    sm64_objects_step(world, terrain, 0);
    assert(sm64_objects_animation_tick(world) == 1 && coin->header.gfx.animInfo.animFrame == 0);
    assert(surface_calls == 1 && player_calls == 1 && tail_calls == 1);
    assert(player->platform == platform && observed_x == 0);
    assert(sm64_terrain_surface_count(terrain) == 1);
    sm64_objects_step(world, terrain, 1);
    assert(observed_x == 5 && sm64_terrain_surface_count(terrain) == 1);
    assert(sm64_objects_previous_count(world) == 4);
    request_stop = 1;
    sm64_objects_step(world, terrain, 2);
    assert(coin->header.gfx.animInfo.animFrame == 2);
    assert(observed_x == 10 && tail_calls == 3);
    assert(sm64_objects_time_stop(world) & TIME_STOP_ACTIVE);
    sm64_objects_step(world, terrain, 3);
    assert(coin->header.gfx.animInfo.animFrame == 2 && coin->header.gfx.animInfo.animTimer == 4);
    assert(surface_calls == 3 && tail_calls == 3 && player_calls == 4);
    assert(observed_x == 10 && sm64_terrain_surface_count(terrain) == 1);
    request_stop = 0;
    sm64_objects_set_time_stop(world, 0);
    sm64_objects_step(world, terrain, 4);
    assert(coin->header.gfx.animInfo.animFrame == 3);
    assert(surface_calls == 4 && tail_calls == 4 && observed_x == 15);
    sm64_objects_destroy(world);
    sm64_terrain_destroy(terrain);
    sm64_audio_destroy(audio);
    puts("Original object frame: dynamic surfaces, platform displacement, collision order and deferred time stop passed");
    return 0;
}
