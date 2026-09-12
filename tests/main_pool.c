#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../host/main_pool.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

int main(void) {
    struct sm64_host_main_pool *a = sm64_host_main_pool_create(4096);
    struct sm64_host_main_pool *b = sm64_host_main_pool_create(4096);
    CHECK(a != NULL && b != NULL);
    CHECK(sm64_host_main_pool_create(32) == NULL);
    CHECK(sm64_host_main_pool_create(SIZE_MAX) == NULL);
    CHECK(sm64_host_main_pool_activate(a) == NULL);
    u32 initial = main_pool_available();
    unsigned char *left = main_pool_alloc(32, MEMORY_POOL_LEFT);
    unsigned char *right = main_pool_alloc(48, MEMORY_POOL_RIGHT);
    CHECK(left != NULL && right != NULL && left < right);
    CHECK((uintptr_t) left % 16 == 0 && (uintptr_t) right % 16 == 0);
    memset(left, 0xa5, 32);
    memset(right, 0x5a, 48);
    CHECK(main_pool_available() == initial - 48 - 64);
    const u32 available_a = main_pool_available();
    CHECK(sm64_host_main_pool_activate(b) == a);
    CHECK(main_pool_available() == initial);
    unsigned char *other = main_pool_alloc(128, MEMORY_POOL_LEFT);
    CHECK(other != NULL && other != left);
    memset(other, 0x3c, 128);
    const u32 available_b = main_pool_available();
    CHECK(sm64_host_main_pool_activate(a) == b);
    CHECK(main_pool_available() == available_a);
    for (int i = 0; i < 32; ++i) CHECK(left[i] == 0xa5);
    for (int i = 0; i < 48; ++i) CHECK(right[i] == 0x5a);
    CHECK(main_pool_alloc(initial, MEMORY_POOL_LEFT) == NULL);
    CHECK(main_pool_available() == available_a);

    main_pool_push_state();
    void *temporary = main_pool_alloc(96, MEMORY_POOL_LEFT);
    CHECK(temporary != NULL);
    main_pool_push_state();
    CHECK(main_pool_alloc(128, MEMORY_POOL_RIGHT) != NULL);
    main_pool_pop_state();
    main_pool_pop_state();
    CHECK(main_pool_available() == available_a);
    CHECK(left[0] == 0xa5 && right[0] == 0x5a);
    /* Upstream pop restores head pointers but not sentinel links. The next
     * allocation on each end renews those links before older blocks are freed. */
    void *renew_left = main_pool_alloc(16, MEMORY_POOL_LEFT);
    void *renew_right = main_pool_alloc(16, MEMORY_POOL_RIGHT);
    CHECK(renew_left != NULL && renew_right != NULL);
    main_pool_free(renew_right);
    main_pool_free(renew_left);
    main_pool_free(right);
    main_pool_free(left);
    CHECK(main_pool_available() == initial);
    left = main_pool_alloc(32, MEMORY_POOL_LEFT);
    memset(left, 0x7b, 32);
    CHECK(main_pool_realloc(left, 128) == left);
    for (int i = 0; i < 32; ++i) CHECK(left[i] == 0x7b);
    main_pool_free(left);
    CHECK(main_pool_available() == initial);

    struct AllocOnlyPool *sub = alloc_only_pool_init(128, MEMORY_POOL_LEFT);
    CHECK(sub != NULL);
    CHECK(alloc_only_pool_alloc(sub, 31) != NULL);
    const s32 used_before_resize = sub->usedSpace;
    CHECK(alloc_only_pool_resize(sub, 256) == sub);
    CHECK(sub->totalSpace == 256 && sub->usedSpace == used_before_resize);
    CHECK(alloc_only_pool_alloc(sub, 128) != NULL);
    main_pool_free(sub);
    CHECK(main_pool_available() == initial);
    sm64_host_main_pool_activate(b);
    CHECK(main_pool_available() == available_b);
    for (int i = 0; i < 128; ++i) CHECK(other[i] == 0x3c);
    sm64_host_main_pool_destroy(a);
    CHECK(main_pool_available() == available_b);
    sm64_host_main_pool_destroy(b);
    CHECK(sm64_host_main_pool_activate(NULL) == NULL);
    puts("Native main pools: independent storage, stack rollback, resize and exhaustion passed");
    return 0;
}
