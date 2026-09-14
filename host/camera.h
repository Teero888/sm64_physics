#ifndef SM64_PHYSICS_CAMERA_H
#define SM64_PHYSICS_CAMERA_H
#include "objects.h"
struct Camera;
/* Camera control is supplied by the host, including for headless simulation.
 * It must synchronously apply the requested mode transition to the borrowed
 * Camera and its own camera state. Mode -1 means restore the previous mode.
 * The callback must not reenter physics. No silent fallback is supplied:
 * Mario reads the resulting mode during subsequent physics updates. */
typedef void (*sm64_camera_mode_callback)(void *user, struct Camera *camera, s16 mode, s16 frames);
void sm64_objects_bind_camera(struct sm64_objects *objects,
    sm64_camera_mode_callback callback, void *user);
#endif
