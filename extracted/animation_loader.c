/* Generated verbatim upstream function extraction. See extracted.json. */
#include "sm64.h"
#include "host/animation_bank.h"

#line 567 "n64decomp/src/game/memory.c"
s32 load_patchable_table(struct DmaHandlerList *list, s32 index) {
    s32 ret = FALSE;
    struct DmaTable *table = list->dmaTable;

    if ((u32)index < table->count) {
        u8 *addr = table->srcAddr + table->anim[index].offset;
        s32 size = table->anim[index].size;

        if (addr != list->currentAddr) {
            dma_read(list->bufTarget, addr, addr + size);
            list->currentAddr = addr;
            ret = TRUE;
        }
    }
    return ret;
}
