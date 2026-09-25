// The sound banks and sequences, from the user's ROM (platform/sound.c).
#pragma once
#include <stdbool.h>
#include <stddef.h>

// Converts a normalized ROM's sound data into the host's layout, for every
// world created after it. False if the ROM has none where this version's is.
bool host_load_sound(const unsigned char *rom, size_t size);
