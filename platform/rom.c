// The user's ROM, for what the library does not carry (game/rom_assets): the
// textures' pixels, which drawing needs. Not part of the state: the process
// keeps one ROM for every world.
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "sm64_physics.h"
#include "rom.h"

// Game code and checksums in the ROM header, big-endian (.z64) byte order.
#if defined(VERSION_JP)
static const char sRomCode[4] = "NSMJ";
static const uint32_t sRomCrc[2] = { 0x4eaa3d0e, 0x74757c24 };
#elif defined(VERSION_EU)
static const char sRomCode[4] = "NSMP";
static const uint32_t sRomCrc[2] = { 0xa03cf036, 0xbcc1c5d2 };
#else
static const char sRomCode[4] = "NSME";
static const uint32_t sRomCrc[2] = { 0x635a2bff, 0x8b022326 };
#endif

// A MIO0 block of the ROM, decompressed.
struct block {
    uint32_t offset;
    uint32_t size;
    unsigned char *data;
};

static mtx_t sLock;
static once_flag sLockOnce = ONCE_FLAG_INIT;
static unsigned char *sRom;
static size_t sRomSize;
static struct block *sBlocks;
static size_t sBlockCount;

static void init_lock(void) {
    mtx_init(&sLock, mtx_plain);
}

static uint32_t read_be32(const unsigned char *p) {
    return (uint32_t) p[0] << 24 | (uint32_t) p[1] << 16 | (uint32_t) p[2] << 8 | p[3];
}

static uint32_t read_le32(const unsigned char *p) {
    return (uint32_t) p[3] << 24 | (uint32_t) p[2] << 16 | (uint32_t) p[1] << 8 | p[0];
}

unsigned char *rom_normalize(const void *data, size_t size) {
    // .z64 is big-endian, .v64 swaps each 16-bit word, .n64 each 32-bit one.
    const unsigned char *in = data;
    if (size < 0x800000 || size % 4 != 0) {
        return NULL;
    }
    int swap;
    if (in[0] == 0x80 && in[1] == 0x37) {
        swap = 0;
    } else if (in[0] == 0x37 && in[1] == 0x80) {
        swap = 1;
    } else if (in[0] == 0x40 && in[3] == 0x80) {
        swap = 3;
    } else {
        return NULL;
    }
    unsigned char *rom = malloc(size);
    if (!rom) {
        return NULL;
    }
    for (size_t i = 0; i < size; ++i) {
        rom[i] = in[swap == 1 ? i ^ 1 : swap == 3 ? i ^ 3 : i];
    }
    if (memcmp(rom + 0x3b, sRomCode, 4) != 0 || read_be32(rom + 0x10) != sRomCrc[0]
        || read_be32(rom + 0x14) != sRomCrc[1]) {
        free(rom);
        return NULL;
    }
    return rom;
}

void rom_keep(unsigned char *rom, size_t size) {
    call_once(&sLockOnce, init_lock);
    mtx_lock(&sLock);
    free(sRom);
    for (size_t i = 0; i < sBlockCount; ++i) {
        free(sBlocks[i].data);
    }
    free(sBlocks);
    sBlocks = NULL;
    sBlockCount = 0;
    sRom = rom;
    sRomSize = size;
    mtx_unlock(&sLock);
}

// MIO0: a header ("MIO0", decompressed size, offsets of the compressed and the
// uncompressed data), then one bit per output step, set for a byte copied as
// it is, clear for a back reference (4 bits length - 3, 12 bits distance - 1).
static unsigned char *mio0_decode(const unsigned char *in, size_t available, uint32_t *size) {
    if (available < 16 || memcmp(in, "MIO0", 4) != 0) {
        return NULL;
    }
    const uint32_t out_size = read_be32(in + 4);
    const uint32_t compressed = read_be32(in + 8), raw = read_be32(in + 12);
    unsigned char *out = malloc(out_size ? out_size : 1);
    if (!out) {
        return NULL;
    }
    uint32_t bits = 16, c = compressed, r = raw, n = 0;
    uint32_t word = 0;
    int left = 0;
    while (n < out_size) {
        if (left == 0) {
            if (bits + 4 > available) {
                break;
            }
            word = read_be32(in + bits);
            bits += 4;
            left = 32;
        }
        const bool literal = (word & 0x80000000u) != 0;
        word <<= 1;
        --left;
        if (literal) {
            if (r >= available) {
                break;
            }
            out[n++] = in[r++];
        } else {
            if (c + 2 > available) {
                break;
            }
            const uint32_t pair = (uint32_t) in[c] << 8 | in[c + 1];
            c += 2;
            const uint32_t length = (pair >> 12) + 3, distance = (pair & 0xfff) + 1;
            if (distance > n) {
                break;
            }
            for (uint32_t i = 0; i < length && n < out_size; ++i, ++n) {
                out[n] = out[n - distance];
            }
        }
    }
    if (n < out_size) {
        free(out);
        return NULL;
    }
    *size = out_size;
    return out;
}

// Called with the lock held.
static const struct block *block_at(uint32_t offset) {
    for (size_t i = 0; i < sBlockCount; ++i) {
        if (sBlocks[i].offset == offset) {
            return &sBlocks[i];
        }
    }
    if (offset >= sRomSize) {
        return NULL;
    }
    struct block block = { offset, 0, NULL };
    block.data = mio0_decode(sRom + offset, sRomSize - offset, &block.size);
    if (!block.data) {
        return NULL;
    }
    struct block *grown = realloc(sBlocks, (sBlockCount + 1) * sizeof(*sBlocks));
    if (!grown) {
        free(block.data);
        return NULL;
    }
    sBlocks = grown;
    sBlocks[sBlockCount] = block;
    return &sBlocks[sBlockCount++];
}

// The largest texture's size: how far before an address its stand-in can start.
#define LARGEST_TEXTURE 8192

// The stand-in an address is in (tools/rom_stubs.py): "sm64", the block
// (never 0), the offset, then zeros. Code also points into textures (a sprite's
// tiles), so the tag can be before the address, past zeros only.
static const unsigned char *stand_in(const unsigned char *address) {
    if (memcmp(address, "sm64", 4) == 0) {
        return address;
    }
    const unsigned char *word = (const unsigned char *) ((uintptr_t) address & ~(uintptr_t) 3);
    for (size_t back = 0; back < LARGEST_TEXTURE; back += 4, word -= 4) {
        uint32_t value;
        memcpy(&value, word, 4);
        if (value == 0) {
            continue;
        }
        // The first word that is not zero is the tag's offset, or its block.
        if (memcmp(word - 8, "sm64", 4) == 0) {
            return word - 8;
        }
        return memcmp(word - 4, "sm64", 4) == 0 ? word - 4 : NULL;
    }
    return NULL;
}

const void *host_texture_pixels(const void *address) {
    const void *pixels = sm64_texture(address);
    return pixels ? pixels : address;
}

const void *sm64_texture(const void *address) {
    const unsigned char *tag = address ? stand_in(address) : NULL;
    if (!tag) {
        return NULL;
    }
    const uint32_t block = read_le32(tag + 4);
    const uint32_t offset = read_le32(tag + 8) + (uint32_t) ((const unsigned char *) address - tag);
    call_once(&sLockOnce, init_lock);
    mtx_lock(&sLock);
    const void *pixels = NULL;
    if (sRom && block == 0xffffffffu) {
        pixels = offset < sRomSize ? sRom + offset : NULL;
    } else if (sRom) {
        const struct block *b = block_at(block);
        pixels = b && offset < b->size ? b->data + offset : NULL;
    }
    mtx_unlock(&sLock);
    return pixels;
}
