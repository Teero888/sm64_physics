/* Generated verbatim upstream function extraction. See extracted.json. */
#include "host/mario_holding_state.h"

#line 267 "n64decomp/src/game/object_helpers.c"
void obj_set_held_state(struct Object *obj, const BehaviorScript *heldBehavior) {
    obj->parentObj = o;

    if (obj->oFlags & OBJ_FLAG_HOLDABLE) {
        if (heldBehavior == bhvCarrySomething3) {
            obj->oHeldState = HELD_HELD;
        }

        if (heldBehavior == bhvCarrySomething5) {
            obj->oHeldState = HELD_THROWN;
        }

        if (heldBehavior == bhvCarrySomething4) {
            obj->oHeldState = HELD_DROPPED;
        }
    } else {
        obj->curBhvCommand = segmented_to_virtual(heldBehavior);
        obj->bhvStackIndex = 0;
    }
}
