#include <assert.h>
#include <stdio.h>
#include "../host/objects.h"
#include "game/object_collision.h"
#include "game/object_list_processor.h"
#include "game/interaction.h"

static const BehaviorScript player[] = {OBJ_LIST_PLAYER << 16};
static const BehaviorScript level[] = {OBJ_LIST_LEVEL << 16};
static const BehaviorScript destructive[] = {OBJ_LIST_DESTRUCTIVE << 16};
static const BehaviorScript surface[] = {OBJ_LIST_SURFACE << 16};
static struct Object *spawn(const BehaviorScript *script, unsigned type, float x) {
    struct Object *obj = sm64_objects_spawn(script);
    assert(obj);
    obj->oIntangibleTimer = 0;
    obj->oInteractType = type;
    obj->oPosX = x;
    return obj;
}
int main(void) {
    struct sm64_objects *a = sm64_objects_create(), *b = sm64_objects_create();
    assert(a && b);
    sm64_objects_activate(a);
    struct Object *mario = spawn(player, 0, 0);
    sm64_objects_set_mario(a, mario);
    struct Object *coin = spawn(level, INTERACT_COIN, 99);
    detect_object_collisions();
    assert(mario->numCollidedObjs == 1 && coin->numCollidedObjs == 1);
    assert(mario->collidedObjs[0] == coin && coin->collidedObjs[0] == mario);
    assert(mario->collidedObjInteractTypes == INTERACT_COIN);
    coin->oPosX = 100;
    detect_object_collisions();
    assert(!mario->numCollidedObjs && !mario->collidedObjInteractTypes);
    coin->oPosX = 0;
    coin->oPosY = 100;
    detect_object_collisions();
    assert(mario->numCollidedObjs == 1); // vertical equality counts
    coin->oPosY = 101;
    detect_object_collisions();
    assert(mario->numCollidedObjs == 0);
    coin->hitboxDownOffset = 1;
    detect_object_collisions();
    assert(mario->numCollidedObjs == 1);
    coin->oPosY = coin->hitboxDownOffset = 0;
    coin->oIntangibleTimer = 2;
    detect_object_collisions();
    assert(coin->oIntangibleTimer == 1 && mario->numCollidedObjs == 0);
    detect_object_collisions();
    assert(coin->oIntangibleTimer == 0 && mario->numCollidedObjs == 1);
    coin->oIntangibleTimer = -1;
    detect_object_collisions();
    assert(coin->oIntangibleTimer == -1 && mario->numCollidedObjs == 0);
    coin->oIntangibleTimer = 0;
    coin->hurtboxRadius = 20;
    coin->hurtboxHeight = 100;
    mario->hurtboxRadius = 20;
    coin->oPosX = 50;
    detect_object_collisions();
    assert(mario->numCollidedObjs == 1);
    assert(coin->oInteractionSubtype & INT_SUBTYPE_DELAY_INVINCIBILITY);
    coin->oPosX = 39;
    detect_object_collisions();
    assert(!(coin->oInteractionSubtype & INT_SUBTYPE_DELAY_INVINCIBILITY));
    struct Object *extras[4];
    for (int i = 0; i < 4; ++i) extras[i] = spawn(level, INTERACT_COIN, 0);
    detect_object_collisions();
    assert(mario->numCollidedObjs == 4 && extras[3]->numCollidedObjs == 0);
    assert(mario->collidedObjs[0] == coin && mario->collidedObjs[3] == extras[2]);

    sm64_objects_activate(b);
    struct Object *attack = spawn(destructive, INTERACT_DAMAGE, 0);
    struct Object *target = spawn(surface, INTERACT_BREAKABLE, 0);
    attack->oDistanceToMario = 2000;
    detect_object_collisions();
    assert(attack->numCollidedObjs == 0);
    attack->oDistanceToMario = 1999;
    detect_object_collisions();
    assert(attack->numCollidedObjs == 1 && target->numCollidedObjs == 1);
    attack->activeFlags |= ACTIVE_FLAG_UNK9;
    detect_object_collisions();
    assert(attack->numCollidedObjs == 0 && target->numCollidedObjs == 0);
    assert(mario->numCollidedObjs == 4); // other context remains untouched
    sm64_objects_destroy(a);
    attack->activeFlags &= ~ACTIVE_FLAG_UNK9;
    detect_object_collisions();
    assert(attack->numCollidedObjs == 1);
    sm64_objects_destroy(b);
    puts("Native object collisions: boundaries, timers, hurtboxes, list priority, capacity and independent contexts passed");
    return 0;
}
