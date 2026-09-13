#include <stdio.h>
#include <stdlib.h>
#include "sm64.h"
#include "game/mario.h"
#include "game/area.h"
#include "../host/animation.h"
#include "../host/animation_bank.h"

u16 gAreaUpdateCounter = 1;
#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

int main(void) {
    s16 values_a[] = {4, 8, 12}, values_b[] = {40, 80, 120};
    u16 indices[] = {1,0, 1,1, 1,2};
    struct sm64_animation_asset assets[] = {
        {.animation = {.flags = ANIM_FLAG_6, .loopEnd = 4, .values = values_a, .index = indices},
         .value_count = 3, .index_count = 6},
        {.animation = {.flags = ANIM_FLAG_6, .loopEnd = 6, .values = values_b, .index = indices},
         .value_count = 3, .index_count = 6}
    };
    struct sm64_animation_bank *a = sm64_animation_bank_create(assets, 2);
    struct sm64_animation_bank *b = sm64_animation_bank_create(assets, 2);
    CHECK(a != NULL && b != NULL);
    values_a[0] = 999; /* Banks own their decoded data. */
    struct Object object = {0}, second_object = {0};
    object.header.gfx.animInfo.animID = second_object.header.gfx.animInfo.animID = -1;
    object.header.gfx.node.flags = second_object.header.gfx.node.flags = GRAPH_RENDER_HAS_ANIMATION;
    struct MarioState mario = {.marioObj = &object, .animList = sm64_animation_bank_handler(a)};
    struct MarioState second = {.marioObj = &second_object, .animList = sm64_animation_bank_handler(b)};
    CHECK(set_mario_animation(&mario, 0) == -1);
    sm64_physics_advance_object_animation(&object.header.gfx);
    CHECK(object.header.gfx.animInfo.animFrame == 0);
    struct Animation *current = object.header.gfx.animInfo.curAnim;
    CHECK(current->values[0] == 4 && current->index[5] == 2);
    const s16 *values_pointer = current->values;
    CHECK(set_mario_animation(&mario, 0) == 0 && current->values == values_pointer);
    update_mario_pos_for_anim(&mario);
    CHECK(mario.pos[0] == 1 && mario.pos[1] == 2 && mario.pos[2] == 3);
    CHECK(return_mario_anim_y_translation(&mario) == 2);
    CHECK(set_mario_animation(&second, 1) == -1);
    CHECK(second_object.header.gfx.animInfo.curAnim != current);
    CHECK(second_object.header.gfx.animInfo.curAnim->values[0] == 40);
    CHECK(current->values[0] == 4);
    CHECK(set_mario_animation(&mario, 1) == -1);
    CHECK(current->values[0] == 40);
    CHECK(set_mario_animation(&mario, 0) == -1 && current->values[0] == 4);
    set_anim_to_frame(&mario, 3);
    CHECK(is_anim_past_frame(&mario, 3));
    ++gAreaUpdateCounter;
    sm64_physics_advance_object_animation(&object.header.gfx);
    CHECK(is_anim_at_end(&mario) && is_anim_past_end(&mario));
    CHECK(set_mario_anim_with_accel(&mario, 1, 0x8000) == -1);
    ++gAreaUpdateCounter;
    sm64_physics_advance_object_animation(&object.header.gfx);
    CHECK(object.header.gfx.animInfo.animFrame == 0);
    sm64_animation_bank_destroy(a);
    ++gAreaUpdateCounter;
    sm64_physics_advance_object_animation(&second_object.header.gfx);
    update_mario_pos_for_anim(&second);
    CHECK(second.pos[0] == 10 && second.pos[1] == 20 && second.pos[2] == 30);
    sm64_animation_bank_destroy(b);
    indices[5] = 3;
    CHECK(sm64_animation_bank_create(assets, 2) == NULL);
    CHECK(sm64_animation_bank_create(NULL, 0) == NULL);
    puts("Native animation banks: relocation, switching, translation and independent lifetimes passed");
    return 0;
}
