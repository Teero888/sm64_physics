#include <stdlib.h>
#include <stdint.h>
#include "main_pool.h"

_Thread_local struct sm64_host_main_pool *sm64_active_main_pool;

struct sm64_host_main_pool *sm64_host_main_pool_activate(struct sm64_host_main_pool *pool) {
    struct sm64_host_main_pool *previous = sm64_active_main_pool;
    sm64_active_main_pool = pool;
    return previous;
}

struct sm64_host_main_pool *sm64_host_main_pool_create(size_t size) {
    if (size < 64 || size > INT32_MAX) {
        return NULL;
    }
    struct sm64_host_main_pool *pool = calloc(1, sizeof(*pool));
    if (pool == NULL) {
        return NULL;
    }
    pool->storage = malloc(size);
    if (pool->storage == NULL) {
        free(pool);
        return NULL;
    }
    pool->storage_size = size;
    struct sm64_host_main_pool *previous = sm64_host_main_pool_activate(pool);
    main_pool_init(pool->storage, (u8 *) pool->storage + size);
    sm64_host_main_pool_activate(previous);
    return pool;
}

void sm64_host_main_pool_destroy(struct sm64_host_main_pool *pool) {
    if (pool == NULL) {
        return;
    }
    if (sm64_active_main_pool == pool) {
        sm64_active_main_pool = NULL;
    }
    free(pool->storage);
    free(pool);
}
