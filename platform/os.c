#include "os.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <malloc.h>
#include <windows.h>

void *os_aligned_alloc(size_t alignment, size_t size) {
    return _aligned_malloc(size, alignment);
}

void os_aligned_free(void *memory) {
    _aligned_free(memory);
}

void os_protect_none(void *memory, size_t size) {
    DWORD old;
    VirtualProtect(memory, size, PAGE_NOACCESS, &old);
}
#else
#include <stdlib.h>
#include <sys/mman.h>

void *os_aligned_alloc(size_t alignment, size_t size) {
    return aligned_alloc(alignment, size);
}

void os_aligned_free(void *memory) {
    free(memory);
}

void os_protect_none(void *memory, size_t size) {
    mprotect(memory, size, PROT_NONE);
}
#endif
