#ifndef SM64_PHYSICS_MEMORY_POOL_H
#define SM64_PHYSICS_MEMORY_POOL_H
#include <stddef.h>
#include "sm64.h"
#include "game/memory.h"

/* Native layouts for upstream's private allocator bookkeeping. Align the
 * header and payload to max_align_t; the N64 ALIGN4 rule cannot safely place
 * pointer-bearing block headers on a 64-bit host. Function bodies are original. */
struct MemoryBlock {
    _Alignas(max_align_t) struct MemoryBlock *next;
    u32 size;
};
struct MemoryPool {
    u32 totalSpace;
    struct MemoryBlock *firstBlock;
    struct MemoryBlock freeList;
};
#define ALIGN4(val) (((val) + _Alignof(max_align_t) - 1) & ~((u32)_Alignof(max_align_t) - 1))

/* mem_pool_init uses the active host main pool and shares its lifetime.
 * Original caller preconditions apply: size >= sizeof(MemoryBlock), no size
 * overflow, and free only live allocations belonging to this pool. Main-pool
 * rollback/free invalidates all contained memory pools and their allocations. */
#endif
