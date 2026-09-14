#include <assert.h>
#include "camera.h"
#include "objects_state.h"
#include "game/camera.h"

void sm64_objects_bind_camera(struct sm64_objects *objects,
    sm64_camera_mode_callback callback, void *user) {
    assert(objects && callback);
    objects->camera_callback = callback;
    objects->camera_user = user;
}

void set_camera_mode(struct Camera *camera, s16 mode, s16 frames) {
    assert(sm64_active_objects && sm64_active_objects->camera_callback && camera);
    sm64_active_objects->camera_callback(sm64_active_objects->camera_user, camera, mode, frames);
}

void set_camera_shake_from_hit(s16 shake) {
    (void)shake;
}

void set_environmental_camera_shake(s16 shake) {
    (void)shake;
}

void set_camera_shake_from_point(s16 shake, f32 posX, f32 posY, f32 posZ) {
    (void)shake; (void)posX; (void)posY; (void)posZ;
}
