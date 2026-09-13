#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "animation_bank.h"

struct sm64_animation_bank {
    struct DmaHandlerList handler;
    struct DmaTable *table;
    u8 *data, *buffer;
};

void dma_read(u8 *destination, u8 *start, u8 *end) {
    memcpy(destination, start, (size_t)(end - start));
}

void sm64_animation_bank_destroy(struct sm64_animation_bank *bank) {
    if (!bank) return;
    free(bank->table);
    free(bank->data);
    free(bank->buffer);
    free(bank);
}

struct DmaHandlerList *sm64_animation_bank_handler(struct sm64_animation_bank *bank) {
    return bank ? &bank->handler : NULL;
}

struct sm64_animation_bank *sm64_animation_bank_create(const struct sm64_animation_asset *assets, size_t count) {
    if (!assets || !count || count > UINT16_MAX) return NULL;
    size_t total = 0, largest = 0;
    for (size_t i = 0; i < count; ++i) {
        const struct sm64_animation_asset *asset = &assets[i];
        if (!asset->animation.values || !asset->animation.index || asset->index_count < 6 ||
            asset->index_count % 2 || asset->value_count > UINT16_MAX || asset->index_count > UINT16_MAX ||
            asset->animation.loopStart < 0 || asset->animation.loopEnd <= asset->animation.loopStart) return NULL;
        for (size_t j = 0; j < asset->index_count; j += 2) {
            size_t frames = asset->animation.index[j], offset = asset->animation.index[j + 1];
            if (!frames || offset + frames > asset->value_count) return NULL;
        }
        size_t size = sizeof(struct Animation) + 2 * (asset->value_count + asset->index_count);
        size = (size + 15) & ~(size_t)15;
        if (size > INT32_MAX || total > INT32_MAX - size) return NULL;
        total += size;
        if (size > largest) largest = size;
    }
    struct sm64_animation_bank *bank = calloc(1, sizeof(*bank));
    if (!bank) return NULL;
    bank->table = calloc(1, offsetof(struct DmaTable, anim) + count * sizeof(struct OffsetSizePair));
    bank->data = calloc(1, total);
    bank->buffer = calloc(1, largest);
    if (!bank->table || !bank->data || !bank->buffer) {
        sm64_animation_bank_destroy(bank);
        return NULL;
    }
    bank->table->count = (u32)count;
    bank->table->srcAddr = bank->data;
    bank->handler.dmaTable = bank->table;
    bank->handler.bufTarget = bank->buffer;
    size_t offset = 0;
    for (size_t i = 0; i < count; ++i) {
        const struct sm64_animation_asset *asset = &assets[i];
        struct Animation native = asset->animation;
        size_t values = sizeof(native), indices = values + 2 * asset->value_count;
        size_t size = (indices + 2 * asset->index_count + 15) & ~(size_t)15;
        /* Retain the original relative-offset representation until the
         * decompiled Mario animation setter relocates the copied buffer. */
        native.values = (const s16 *)(uintptr_t)values;
        native.index = (const u16 *)(uintptr_t)indices;
        memcpy(bank->data + offset, &native, sizeof(native));
        memcpy(bank->data + offset + values, asset->animation.values, 2 * asset->value_count);
        memcpy(bank->data + offset + indices, asset->animation.index, 2 * asset->index_count);
        bank->table->anim[i].offset = (u32)offset;
        bank->table->anim[i].size = (u32)size;
        offset += size;
    }
    return bank;
}
