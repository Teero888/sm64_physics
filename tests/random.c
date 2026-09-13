#include <assert.h>
#include <stdio.h>
#include "../host/objects.h"
#include "engine/behavior_script.h"

int main(void) {
    struct sm64_objects *a = sm64_objects_create(), *b = sm64_objects_create();
    assert(a && b);
    sm64_objects_activate(a);
    assert(random_u16() == 0xe074);
    sm64_objects_set_random_seed(a, 22026);
    assert(random_u16() == 0xe074); // original special-seed reset
    for (int i = 0; i < 1000; ++i) random_u16();
    u16 checkpoint = sm64_objects_random_seed(a);
    u16 expected[1000];
    for (int i = 0; i < 1000; ++i) expected[i] = random_u16();
    u16 final = sm64_objects_random_seed(a);
    sm64_objects_activate(b);
    assert(sm64_objects_random_seed(b) == 0);
    sm64_objects_set_random_seed(b, checkpoint);
    for (int i = 0; i < 1000; ++i) assert(random_u16() == expected[i]);
    assert(sm64_objects_random_seed(b) == final);
    sm64_objects_destroy(a);
    for (unsigned seed = 0; seed <= 65535; ++seed) {
        sm64_objects_set_random_seed(b, seed);
        float value = random_float();
        assert(value >= 0 && value < 1);
        int sign = random_sign();
        assert(sign == -1 || sign == 1);
    }
    sm64_objects_destroy(b);
    puts("Native random state: original seed reset, replay, independent owners and output domains passed");
    return 0;
}
