// What the library needs of the operating system beyond C and pthreads
// (platform/os.c): memory aligned to 64 KiB, and taking all access to memory
// away.
#pragma once
#include <stddef.h>

void *os_aligned_alloc(size_t alignment, size_t size);
void os_aligned_free(void *memory);
// Any access to [memory, memory + size) faults from then on; page aligned.
void os_protect_none(void *memory, size_t size);
