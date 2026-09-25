// The user's ROM, kept by the process (platform/rom.c).
#pragma once
#include <stddef.h>

// The ROM in big-endian (.z64) byte order, if it is this version's, or NULL.
unsigned char *rom_normalize(const void *data, size_t size);
// Keeps a normalized ROM for the textures (sm64_texture), replacing the last.
void rom_keep(unsigned char *rom, size_t size);
