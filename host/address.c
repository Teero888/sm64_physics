#include "game/memory.h"

/* FrameTee resolves ROM segment addresses while decoding host assets. Physics
 * receives ordinary native pointers and never accesses a ROM or an RSP. */
void *segmented_to_virtual(const void *address) {
    return (void *) address;
}

void *virtual_to_segmented(u32 segment, const void *address) {
    (void) segment;
    return (void *) address;
}
