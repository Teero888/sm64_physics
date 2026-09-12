#include <stdint.h>
#include <stddef.h>
#include "game/memory.h"

/* Host allocation only. Align native pointers correctly even when a previous
 * request had an odd size; the original N64 pool rounded requests to 4 bytes.
 * Storage and this descriptor belong to the calling world. */
void *alloc_only_pool_alloc(struct AllocOnlyPool *pool, s32 size) {
    const uintptr_t alignment = _Alignof(max_align_t);
    if (pool == NULL || size <= 0 || pool->usedSpace < 0 ||
        pool->totalSpace < pool->usedSpace) {
        return NULL;
    }
    const uintptr_t current = (uintptr_t) pool->freePtr;
    const size_t padding = (alignment - current % alignment) % alignment;
    const size_t remaining = (size_t) (pool->totalSpace - pool->usedSpace);
    if (padding > remaining || (size_t) size > remaining - padding) {
        return NULL;
    }
    void *result = pool->freePtr + padding;
    pool->freePtr += padding + (size_t) size;
    pool->usedSpace += (s32) (padding + (size_t) size);
    return result;
}
