#include <PR/ultratypes.h>

#include "sm64.h"
#include "debug.h"
#include "interaction.h"
#include "mario.h"
#include "object_list_processor.h"
#include "spawn_object.h"

struct Object *debug_print_obj_collision(struct Object *a) {
    struct Object *sp24;
    UNUSED u8 filler[4];
    s32 i;

    for (i = 0; i < a->numCollidedObjs; i++) {
        print_debug_top_down_objectinfo("ON", 0);
        sp24 = a->collidedObjs[i];
        if (sp24 != WORLD(gMarioObject)) {
            return sp24;
        }
    }
    return NULL;
}

#ifdef AVOID_UB
// On the N64 the two overlap tests below fall off their end without a return
// value when the objects do not overlap, and return whatever the v0 register
// holds. Within a collision pass nothing else writes v0 between their calls
// (the check_* and clear_* functions do not, and sqrtf only uses float
// registers), so that is the previous value either of them returned. A pass
// starts with v0 holding the upper half of get_clock_difference's s64 result,
// called just before detect_object_collisions in update_objects: 0 for any
// frame shorter than 2^32 CPU cycles. Once one test has found an overlap,
// every later miss in the pass counts as a hit.
//
// EU is built with optimization and differs: clear_object_collision walks its
// list in v0 and leaves the list head's address there, so a pass starts with
// v0 nonzero, and detect_object_hurtbox_overlap loads &gMarioObject into v0
// before its test, so its miss returns that. Only whether v0 is zero matters
// to the callers.
static s32 sCollisionV0;
#define RETURN_V0(value) return (WORLD(sCollisionV0) = (value))
#else
#define RETURN_V0(value) return (value)
#endif

s32 detect_object_hitbox_overlap(struct Object *a, struct Object *b) {
    f32 sp3C = a->oPosY - a->hitboxDownOffset;
    f32 sp38 = b->oPosY - b->hitboxDownOffset;
    f32 dx = a->oPosX - b->oPosX;
    UNUSED f32 sp30 = sp3C - sp38;
    f32 dz = a->oPosZ - b->oPosZ;
    f32 collisionRadius = a->hitboxRadius + b->hitboxRadius;
    f32 distance = sqrtf(dx * dx + dz * dz);

    if (collisionRadius > distance) {
        f32 sp20 = a->hitboxHeight + sp3C;
        f32 sp1C = b->hitboxHeight + sp38;

        if (sp3C > sp1C) {
            RETURN_V0(0);
        }
        if (sp20 < sp38) {
            RETURN_V0(0);
        }
        if (a->numCollidedObjs >= 4) {
            RETURN_V0(0);
        }
        if (b->numCollidedObjs >= 4) {
            RETURN_V0(0);
        }
        a->collidedObjs[a->numCollidedObjs] = b;
        b->collidedObjs[b->numCollidedObjs] = a;
        a->collidedObjInteractTypes |= b->oInteractType;
        b->collidedObjInteractTypes |= a->oInteractType;
        a->numCollidedObjs++;
        b->numCollidedObjs++;
        RETURN_V0(1);
    }

    //! no return value
#ifdef AVOID_UB
    return WORLD(sCollisionV0);
#endif
}

s32 detect_object_hurtbox_overlap(struct Object *a, struct Object *b) {
    f32 sp3C = a->oPosY - a->hitboxDownOffset;
    f32 sp38 = b->oPosY - b->hitboxDownOffset;
    f32 sp34 = a->oPosX - b->oPosX;
    UNUSED f32 sp30 = sp3C - sp38;
    f32 sp2C = a->oPosZ - b->oPosZ;
    f32 sp28 = a->hurtboxRadius + b->hurtboxRadius;
    f32 sp24 = sqrtf(sp34 * sp34 + sp2C * sp2C);
#if defined(AVOID_UB) && defined(VERSION_EU)
    WORLD(sCollisionV0) = 1; // &gMarioObject
#endif

    if (a == WORLD(gMarioObject)) {
        b->oInteractionSubtype |= INT_SUBTYPE_DELAY_INVINCIBILITY;
    }

    if (sp28 > sp24) {
        f32 sp20 = a->hitboxHeight + sp3C;
        f32 sp1C = b->hurtboxHeight + sp38;

        if (sp3C > sp1C) {
            RETURN_V0(0);
        }
        if (sp20 < sp38) {
            RETURN_V0(0);
        }
        if (a == WORLD(gMarioObject)) {
            b->oInteractionSubtype &= ~INT_SUBTYPE_DELAY_INVINCIBILITY;
        }
        RETURN_V0(1);
    }

    //! no return value
#ifdef AVOID_UB
    return WORLD(sCollisionV0);
#endif
}

void clear_object_collision(struct Object *a) {
    struct Object *sp4 = (struct Object *) a->header.next;

    while (sp4 != a) {
        sp4->numCollidedObjs = 0;
        sp4->collidedObjInteractTypes = 0;
        if (sp4->oIntangibleTimer > 0) {
            sp4->oIntangibleTimer--;
        }
        sp4 = (struct Object *) sp4->header.next;
    }
}

void check_collision_in_list(struct Object *a, struct Object *b, struct Object *c) {
    if (a->oIntangibleTimer == 0) {
        while (b != c) {
            if (b->oIntangibleTimer == 0) {
                if (detect_object_hitbox_overlap(a, b) && b->hurtboxRadius != 0.0f) {
                    detect_object_hurtbox_overlap(a, b);
                }
            }
            b = (struct Object *) b->header.next;
        }
    }
}

void check_player_object_collision(void) {
    struct Object *sp1C = (struct Object *) &WORLD(gObjectLists)[OBJ_LIST_PLAYER];
    struct Object *sp18 = (struct Object *) sp1C->header.next;

    while (sp18 != sp1C) {
        check_collision_in_list(sp18, (struct Object *) sp18->header.next, sp1C);
        check_collision_in_list(sp18, (struct Object *) WORLD(gObjectLists)[OBJ_LIST_POLELIKE].next,
                      (struct Object *) &WORLD(gObjectLists)[OBJ_LIST_POLELIKE]);
        check_collision_in_list(sp18, (struct Object *) WORLD(gObjectLists)[OBJ_LIST_LEVEL].next,
                      (struct Object *) &WORLD(gObjectLists)[OBJ_LIST_LEVEL]);
        check_collision_in_list(sp18, (struct Object *) WORLD(gObjectLists)[OBJ_LIST_GENACTOR].next,
                      (struct Object *) &WORLD(gObjectLists)[OBJ_LIST_GENACTOR]);
        check_collision_in_list(sp18, (struct Object *) WORLD(gObjectLists)[OBJ_LIST_PUSHABLE].next,
                      (struct Object *) &WORLD(gObjectLists)[OBJ_LIST_PUSHABLE]);
        check_collision_in_list(sp18, (struct Object *) WORLD(gObjectLists)[OBJ_LIST_SURFACE].next,
                      (struct Object *) &WORLD(gObjectLists)[OBJ_LIST_SURFACE]);
        check_collision_in_list(sp18, (struct Object *) WORLD(gObjectLists)[OBJ_LIST_DESTRUCTIVE].next,
                      (struct Object *) &WORLD(gObjectLists)[OBJ_LIST_DESTRUCTIVE]);
        sp18 = (struct Object *) sp18->header.next;
    }
}

void check_pushable_object_collision(void) {
    struct Object *sp1C = (struct Object *) &WORLD(gObjectLists)[OBJ_LIST_PUSHABLE];
    struct Object *sp18 = (struct Object *) sp1C->header.next;

    while (sp18 != sp1C) {
        check_collision_in_list(sp18, (struct Object *) sp18->header.next, sp1C);
        sp18 = (struct Object *) sp18->header.next;
    }
}

void check_destructive_object_collision(void) {
    struct Object *sp1C = (struct Object *) &WORLD(gObjectLists)[OBJ_LIST_DESTRUCTIVE];
    struct Object *sp18 = (struct Object *) sp1C->header.next;

    while (sp18 != sp1C) {
        if (sp18->oDistanceToMario < 2000.0f && !(sp18->activeFlags & ACTIVE_FLAG_UNK9)) {
            check_collision_in_list(sp18, (struct Object *) sp18->header.next, sp1C);
            check_collision_in_list(sp18, (struct Object *) WORLD(gObjectLists)[OBJ_LIST_GENACTOR].next,
                          (struct Object *) &WORLD(gObjectLists)[OBJ_LIST_GENACTOR]);
            check_collision_in_list(sp18, (struct Object *) WORLD(gObjectLists)[OBJ_LIST_PUSHABLE].next,
                          (struct Object *) &WORLD(gObjectLists)[OBJ_LIST_PUSHABLE]);
            check_collision_in_list(sp18, (struct Object *) WORLD(gObjectLists)[OBJ_LIST_SURFACE].next,
                          (struct Object *) &WORLD(gObjectLists)[OBJ_LIST_SURFACE]);
        }
        sp18 = (struct Object *) sp18->header.next;
    }
}

void detect_object_collisions(void) {
#if defined(AVOID_UB) && defined(VERSION_EU)
    WORLD(sCollisionV0) = 1; // the last list head clear_object_collision walked
#elif defined(AVOID_UB)
    WORLD(sCollisionV0) = 0;
#endif
    clear_object_collision((struct Object *) &WORLD(gObjectLists)[OBJ_LIST_POLELIKE]);
    clear_object_collision((struct Object *) &WORLD(gObjectLists)[OBJ_LIST_PLAYER]);
    clear_object_collision((struct Object *) &WORLD(gObjectLists)[OBJ_LIST_PUSHABLE]);
    clear_object_collision((struct Object *) &WORLD(gObjectLists)[OBJ_LIST_GENACTOR]);
    clear_object_collision((struct Object *) &WORLD(gObjectLists)[OBJ_LIST_LEVEL]);
    clear_object_collision((struct Object *) &WORLD(gObjectLists)[OBJ_LIST_SURFACE]);
    clear_object_collision((struct Object *) &WORLD(gObjectLists)[OBJ_LIST_DESTRUCTIVE]);
    check_player_object_collision();
    check_destructive_object_collision();
    check_pushable_object_collision();
}

// Library: its variables' addresses (tools/state/types.py).
#include "pointers/game/src/game/object_collision.c.inc.c"
