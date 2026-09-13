/* Generated verbatim upstream function extraction. See extracted.json. */
#include "host/object_frame_state.h"

#line 626 "n64decomp/src/game/object_list_processor.c"
void update_objects(UNUSED s32 unused) {
    s64 cycleCounts[30];

    cycleCounts[0] = get_current_clock();

    gTimeStopState &= ~TIME_STOP_MARIO_OPENED_DOOR;

    gNumRoomedObjectsInMarioRoom = 0;
    gNumRoomedObjectsNotInMarioRoom = 0;
    gCheckingSurfaceCollisionsForCamera = FALSE;

    reset_debug_objectinfo();
    stub_debug_5();

    gObjectLists = gObjectListArray;

    // If time stop is not active, unload object surfaces
    cycleCounts[1] = get_clock_difference(cycleCounts[0]);
    clear_dynamic_surfaces();

    // Update spawners and objects with surfaces
    cycleCounts[2] = get_clock_difference(cycleCounts[0]);
    update_terrain_objects();

    // If Mario was touching a moving platform at the end of last frame, apply
    // displacement now
    //! If the platform object unloaded and a different object took its place,
    //  displacement could be applied incorrectly
    apply_mario_platform_displacement();

    // Detect which objects are intersecting
    cycleCounts[3] = get_clock_difference(cycleCounts[0]);
    detect_object_collisions();

    // Update all other objects that haven't been updated yet
    cycleCounts[4] = get_clock_difference(cycleCounts[0]);
    update_non_terrain_objects();

    // Unload any objects that have been deactivated
    cycleCounts[5] = get_clock_difference(cycleCounts[0]);
    unload_deactivated_objects();

    // Check if Mario is on a platform object and save this object
    cycleCounts[6] = get_clock_difference(cycleCounts[0]);
    update_mario_platform();

    cycleCounts[7] = get_clock_difference(cycleCounts[0]);

    cycleCounts[0] = 0;
    try_print_debug_mario_object_info();

    // If time stop was enabled this frame, activate it now so that it will
    // take effect next frame
    if (gTimeStopState & TIME_STOP_ENABLED) {
        gTimeStopState |= TIME_STOP_ACTIVE;
    } else {
        gTimeStopState &= ~TIME_STOP_ACTIVE;
    }

    gPrevFrameObjectCount = gObjectCounter;
}
