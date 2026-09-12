#ifndef SM64_PHYSICS_MAIN_POOL_H
#define SM64_PHYSICS_MAIN_POOL_H
#include <stddef.h>
#include "sm64.h"
#include "game/memory.h"

/* Original bookkeeping layouts from src/game/memory.c. */
struct MainPoolState {
    u32 freeSpace;
    struct MainPoolBlock *listHeadL;
    struct MainPoolBlock *listHeadR;
    struct MainPoolState *prev;
};
struct MainPoolBlock {
    struct MainPoolBlock *prev;
    struct MainPoolBlock *next;
};

/* Host ownership of the globals used by the original pool algorithms. This
 * is one component of a future world, not a complete simulation snapshot. */
struct sm64_host_main_pool {
    void *storage;
    size_t storage_size;
    u32 free_space;
    u8 *start, *end;
    struct MainPoolBlock *left, *right;
    struct MainPoolState *saved;
};

struct sm64_host_main_pool *sm64_host_main_pool_create(size_t size);
void sm64_host_main_pool_destroy(struct sm64_host_main_pool *pool);
/* Returns the previous pool so callers can restore a nested activation. A pool
 * may be active on only one thread at a time. All native main_pool_* calls
 * require a non-null active pool and valid upstream allocation arguments. */
struct sm64_host_main_pool *sm64_host_main_pool_activate(struct sm64_host_main_pool *pool);

#ifdef SM64_PHYSICS_POOL_IMPLEMENTATION
extern _Thread_local struct sm64_host_main_pool *sm64_active_main_pool;
#define ALIGN4(val) (((val) + 0x3) & ~0x3)
#define ALIGN16(val) (((val) + 0xF) & ~0xF)
#define sPoolFreeSpace (sm64_active_main_pool->free_space)
#define sPoolStart (sm64_active_main_pool->start)
#define sPoolEnd (sm64_active_main_pool->end)
#define sPoolListHeadL (sm64_active_main_pool->left)
#define sPoolListHeadR (sm64_active_main_pool->right)
#define gMainPoolState (sm64_active_main_pool->saved)
#endif
#endif
