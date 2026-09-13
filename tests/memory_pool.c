#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../host/main_pool.h"
#include "../host/memory_pool.h"

int main(void) {
    struct sm64_host_main_pool *owner = sm64_host_main_pool_create(16384);
    assert(owner);
    sm64_host_main_pool_activate(owner);
    struct MemoryPool *left = mem_pool_init(4096, MEMORY_POOL_LEFT);
    struct MemoryPool *right = mem_pool_init(2048, MEMORY_POOL_RIGHT);
    assert(left && right);
    assert(mem_pool_init(32768, MEMORY_POOL_LEFT) == NULL);

    void *blocks[64];
    for (int i = 0; i < 64; ++i) {
        blocks[i] = mem_pool_alloc(left, 1 + i % 23);
        assert(blocks[i] && (uintptr_t)blocks[i] % _Alignof(max_align_t) == 0);
        memset(blocks[i], i, 1 + i % 23);
    }
    void *other = mem_pool_alloc(right, 100);
    assert(other);
    memset(other, 0x5a, 100);
    for (int i = 0; i < 64; ++i) {
        for (int j = 0; j < 1 + i % 23; ++j) assert(((unsigned char *)blocks[i])[j] == i);
    }
    // Fragmented frees exercise insertion before, between and after blocks.
    for (int i = 1; i < 64; i += 2) mem_pool_free(left, blocks[i]);
    for (int i = 62; i >= 0; i -= 2) mem_pool_free(left, blocks[i]);
    assert(left->freeList.next == left->firstBlock);
    assert(left->firstBlock->size == 4096 && left->firstBlock->next == NULL);
    for (int j = 0; j < 100; ++j) assert(((unsigned char *)other)[j] == 0x5a);

    // Consume the entire free list, then free into an empty list and reuse it.
    void *whole = mem_pool_alloc(left, 4096 - sizeof(struct MemoryBlock));
    assert(whole && left->freeList.next == NULL);
    assert(mem_pool_alloc(left, 1) == NULL);
    mem_pool_free(left, whole);
    assert(mem_pool_alloc(left, 4096 - sizeof(struct MemoryBlock)) == whole);
    mem_pool_free(left, whole);

    // A tail too small for another header is absorbed into this allocation.
    void *almost = mem_pool_alloc(left, 4096 - 2 * sizeof(struct MemoryBlock));
    assert(almost && left->freeList.next == NULL);
    mem_pool_free(left, almost);
    assert(left->firstBlock->size == 4096);
    mem_pool_free(right, other);
    assert(right->firstBlock->size == 2048);

    struct sm64_host_main_pool *second = sm64_host_main_pool_create(4096);
    assert(second);
    assert(sm64_host_main_pool_activate(second) == owner);
    struct MemoryPool *independent = mem_pool_init(1024, MEMORY_POOL_LEFT);
    assert(independent);
    void *value = mem_pool_alloc(independent, sizeof(long double));
    assert(value);
    *(long double *)value = 1.25L;
    sm64_host_main_pool_destroy(owner);
    assert(*(long double *)value == 1.25L);
    mem_pool_free(independent, value);
    assert(independent->firstBlock->size == 1024);
    sm64_host_main_pool_destroy(second);
    puts("Native memory pools: alignment, fragmentation, exhaustion, coalescing and independent owners passed");
    return 0;
}
