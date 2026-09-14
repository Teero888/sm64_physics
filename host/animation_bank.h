#ifndef SM64_PHYSICS_ANIMATION_BANK_H
#define SM64_PHYSICS_ANIMATION_BANK_H
#include <stddef.h>
#include "game/memory.h"

struct sm64_animation_asset {
    struct Animation animation;
    size_t value_count, index_count;
};
struct sm64_animation_bank;
/* Deep-copies decoded host animations. The host supplies no ROM addresses. */
struct sm64_animation_bank *sm64_animation_bank_create(const struct sm64_animation_asset *assets, size_t count);
struct sm64_animation_bank *sm64_animation_bank_clone(const struct sm64_animation_bank *bank);
struct sm64_animation_bank *sm64_animation_bank_default(void);
void sm64_animation_bank_destroy(struct sm64_animation_bank *bank);
struct DmaHandlerList *sm64_animation_bank_handler(struct sm64_animation_bank *bank);
/* Private host-memory replacement for N64 DMA, used by the original loader. */
void dma_read(u8 *destination, u8 *start, u8 *end);
#endif
