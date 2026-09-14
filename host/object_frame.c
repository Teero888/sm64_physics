#include <assert.h>
#include <time.h>
#include "object_frame.h"
#include "object_frame_state.h"
#include "animation.h"

u64 sm64_frame_clock(void) {
    struct timespec value = {0};
    timespec_get(&value, TIME_UTC);
    return (u64)value.tv_sec * 1000000000 + value.tv_nsec;
}
u64 sm64_frame_elapsed(u64 start) { return sm64_frame_clock() - start; }
void sm64_frame_reset_diagnostics(void) {
    sm64_active_objects->object_counter = 0;
    sm64_active_terrain->floor_misses = 0;
}
/* There is no on-screen N64 debug overlay in this physics library. */
void sm64_frame_debug_output(void) {}
void try_print_debug_mario_level_info(void) {}
void try_do_mario_debug_object_spawn(void) {}

u32 sm64_objects_time_stop(const struct sm64_objects *objects) { assert(objects); return objects->time_stop; }
void sm64_objects_set_time_stop(struct sm64_objects *objects, u32 flags) { assert(objects); objects->time_stop = flags; }
u32 sm64_objects_previous_count(const struct sm64_objects *objects) { assert(objects); return objects->previous_object_count; }

void sm64_objects_step(struct sm64_objects *objects, struct sm64_terrain *terrain, u32 frame) {
    assert(objects && terrain);
    struct sm64_objects *previous_objects = sm64_objects_activate(objects);
    struct sm64_terrain *previous_terrain = sm64_terrain_activate(terrain);
    struct Object **previous_current = terrain->current_ref;
    struct Object **previous_mario_object = terrain->mario_object_ref;
    struct MarioState **previous_mario = terrain->mario_ref;
    u32 *previous_stop = terrain->time_stop_ref;
    terrain->current_ref = &objects->current;
    terrain->mario_object_ref = &objects->mario;
    terrain->mario_ref = &objects->mario_state;
    terrain->time_stop_ref = &objects->time_stop;
    objects->global_timer = frame;
    terrain->level_num = objects->level;
    gMarioObject = objects->mario;
    /* Original area_update_objects increments this before update_objects. */
    ++objects->animation_tick;
    update_objects(0);
    objects->current = gCurrentObject;
    /* Animation state used by the next behavior tick must progress without
     * invoking a renderer. Frozen objects have HAS_ANIMATION cleared by the
     * original scheduler; the helper still stamps their animation timer. */
    for (size_t i = 0; i < OBJECT_POOL_CAPACITY; ++i) {
        struct Object *object = &objects->pool[i];
        if (object->activeFlags && (object->header.gfx.node.flags & GRAPH_RENDER_ACTIVE)) {
            sm64_physics_advance_object_animation(&object->header.gfx);
        }
    }
    terrain->time_stop = objects->time_stop;
    terrain->current_ref = previous_current;
    terrain->mario_object_ref = previous_mario_object;
    terrain->mario_ref = previous_mario;
    terrain->time_stop_ref = previous_stop;
    sm64_terrain_activate(previous_terrain);
    sm64_objects_activate(previous_objects);
}
