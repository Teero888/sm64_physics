#define _GNU_SOURCE
#include "elf_symbols.h"

#ifndef _WIN32
#include <elf.h>
#include <link.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *sImage;
static size_t sImageSize;
static uintptr_t sLoadBase;

static int find_base(struct dl_phdr_info *info, size_t size, void *data) {
    (void) size;
    (void) data;
    // The first object is the executable.
    sLoadBase = info->dlpi_addr;
    return 1;
}

static int load_image(void) {
    if (sImage) {
        return 1;
    }
    FILE *f = fopen("/proc/self/exe", "rb");
    if (!f) {
        return 0;
    }
    fseek(f, 0, SEEK_END);
    sImageSize = (size_t) ftell(f);
    fseek(f, 0, SEEK_SET);
    sImage = malloc(sImageSize);
    if (!sImage || fread(sImage, 1, sImageSize, f) != sImageSize) {
        fclose(f);
        free(sImage);
        sImage = NULL;
        return 0;
    }
    fclose(f);
    dl_iterate_phdr(find_base, NULL);
    return 1;
}

void *elf_symbol(const char *name, size_t *size) {
    if (!load_image()) {
        return NULL;
    }
    const char *colon = strchr(name, ':');
    char file[256] = "";
    if (colon) {
        snprintf(file, sizeof(file), "%.*s", (int) (colon - name), name);
        name = colon + 1;
    }
    const Elf64_Ehdr *header = (const Elf64_Ehdr *) sImage;
    const Elf64_Shdr *sections = (const Elf64_Shdr *) (sImage + header->e_shoff);
    for (int s = 0; s < header->e_shnum; ++s) {
        if (sections[s].sh_type != SHT_SYMTAB) {
            continue;
        }
        const Elf64_Sym *symbols = (const Elf64_Sym *) (sImage + sections[s].sh_offset);
        const char *strings = (const char *) (sImage + sections[sections[s].sh_link].sh_offset);
        const size_t count = sections[s].sh_size / sizeof(Elf64_Sym);
        const char *current_file = "";
        for (size_t i = 0; i < count; ++i) {
            const char *symbol = strings + symbols[i].st_name;
            if (ELF64_ST_TYPE(symbols[i].st_info) == STT_FILE) {
                // Paths are as compiled; compare the file name only.
                const char *slash = strrchr(symbol, '/');
                current_file = slash ? slash + 1 : symbol;
                continue;
            }
            if (ELF64_ST_TYPE(symbols[i].st_info) != STT_OBJECT || strcmp(symbol, name) != 0) {
                continue;
            }
            if (file[0] && strcmp(file, current_file) != 0) {
                continue;
            }
            if (size) {
                *size = symbols[i].st_size;
            }
            return (void *) (sLoadBase + symbols[i].st_value);
        }
    }
    return NULL;
}

typedef struct {
    uintptr_t address;
    size_t size;
    const char *name;
} sorted_symbol;

static sorted_symbol *sSorted;
static size_t sSortedCount;

static int compare_symbols(const void *a, const void *b) {
    const sorted_symbol *x = a, *y = b;
    return x->address < y->address ? -1 : x->address > y->address;
}

static void build_sorted(void) {
    const Elf64_Ehdr *header = (const Elf64_Ehdr *) sImage;
    const Elf64_Shdr *sections = (const Elf64_Shdr *) (sImage + header->e_shoff);
    for (int s = 0; s < header->e_shnum; ++s) {
        if (sections[s].sh_type != SHT_SYMTAB) {
            continue;
        }
        const Elf64_Sym *symbols = (const Elf64_Sym *) (sImage + sections[s].sh_offset);
        const char *strings = (const char *) (sImage + sections[sections[s].sh_link].sh_offset);
        const size_t count = sections[s].sh_size / sizeof(Elf64_Sym);
        sSorted = calloc(count, sizeof(*sSorted));
        for (size_t i = 0; i < count; ++i) {
            const int type = ELF64_ST_TYPE(symbols[i].st_info);
            if ((type != STT_OBJECT && type != STT_FUNC) || symbols[i].st_size == 0) {
                continue;
            }
            sSorted[sSortedCount++] = (sorted_symbol) { sLoadBase + symbols[i].st_value, symbols[i].st_size,
                                                        strings + symbols[i].st_name };
        }
        qsort(sSorted, sSortedCount, sizeof(*sSorted), compare_symbols);
        return;
    }
}

const char *elf_symbol_containing(const void *addr, size_t *offset, size_t *size) {
    if (!load_image()) {
        return NULL;
    }
    if (!sSorted) {
        build_sorted();
    }
    const uintptr_t a = (uintptr_t) addr;
    size_t low = 0, high = sSortedCount;
    while (low < high) {
        const size_t mid = (low + high) / 2;
        if (sSorted[mid].address <= a) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    if (low == 0) {
        return NULL;
    }
    const sorted_symbol *found = &sSorted[low - 1];
    if (a >= found->address + found->size) {
        return NULL;
    }
    *offset = a - found->address;
    *size = found->size;
    return found->name;
}
#else
// Windows: the executable's COFF symbol table, which MinGW's linker keeps. It
// has no sizes: a symbol ends where the next one of its section starts.
#define WIN32_LEAN_AND_MEAN
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef struct {
    uintptr_t address;
    size_t size;
    char *name;
    const char *file;
    int section, data;
} pe_symbol;

static pe_symbol *sSymbols;
static size_t sSymbolCount;

static uint32_t u32(const unsigned char *p) {
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t) p[3] << 24;
}

static uint16_t u16(const unsigned char *p) {
    return (uint16_t) (p[0] | p[1] << 8);
}

static int compare_addresses(const void *a, const void *b) {
    const pe_symbol *x = a, *y = b;
    return x->address < y->address ? -1 : x->address > y->address;
}

static int load_symbols(void) {
    if (sSymbols) {
        return 1;
    }
    char path[MAX_PATH];
    if (!GetModuleFileNameA(NULL, path, sizeof(path))) {
        return 0;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        return 0;
    }
    fseek(f, 0, SEEK_END);
    const size_t size = (size_t) ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *image = malloc(size);
    if (!image || fread(image, 1, size, f) != size) {
        fclose(f);
        free(image);
        return 0;
    }
    fclose(f);
    const uintptr_t base = (uintptr_t) GetModuleHandleA(NULL);
    const unsigned char *pe = image + u32(image + 0x3c) + 4;
    const unsigned sections = u16(pe + 2), optional = u16(pe + 16);
    const unsigned char *table = image + u32(pe + 8);
    const uint32_t count = u32(pe + 12);
    const unsigned char *strings = table + 18 * count;
    const unsigned char *headers = pe + 20 + optional;
    sSymbols = calloc(count ? count : 1, sizeof(*sSymbols));
    const char *file = "";
    for (uint32_t i = 0; i < count; ++i) {
        const unsigned char *entry = table + 18 * i;
        const int section = (int16_t) u16(entry + 12);
        const unsigned type = u16(entry + 14), storage = entry[16], aux = entry[17];
        char name[256];
        if (u32(entry) == 0) {
            snprintf(name, sizeof(name), "%s", (const char *) strings + u32(entry + 4));
        } else {
            snprintf(name, sizeof(name), "%.8s", (const char *) entry);
        }
        if (storage == 103 && aux > 0) { // C_FILE: the name is in the auxiliary records
            char *copy = calloc(1, 18 * aux + 1);
            memcpy(copy, entry + 18, 18 * aux);
            const char *slash = strrchr(copy, '/');
            const char *backslash = strrchr(copy, '\\');
            file = backslash && (!slash || backslash > slash) ? backslash + 1 : slash ? slash + 1 : copy;
        } else if (section > 0 && (unsigned) section <= sections && (storage == 2 || storage == 3) && name[0] != '.') {
            const unsigned char *header = headers + 40 * (section - 1);
            pe_symbol *symbol = &sSymbols[sSymbolCount++];
            symbol->address = base + u32(header + 12) + u32(entry + 8);
            symbol->name = strdup(name);
            symbol->file = file;
            symbol->section = section;
            symbol->data = (type >> 4) != 2; // not a function
        }
        i += aux;
    }
    qsort(sSymbols, sSymbolCount, sizeof(*sSymbols), compare_addresses);
    for (size_t i = 0; i < sSymbolCount; ++i) {
        size_t next = i + 1;
        while (next < sSymbolCount && sSymbols[next].address == sSymbols[i].address) {
            ++next;
        }
        sSymbols[i].size = next < sSymbolCount && sSymbols[next].section == sSymbols[i].section
                               ? sSymbols[next].address - sSymbols[i].address
                               : 0;
    }
    free(image);
    return 1;
}

void *elf_symbol(const char *name, size_t *size) {
    if (!load_symbols()) {
        return NULL;
    }
    const char *colon = strchr(name, ':');
    char file[256] = "";
    if (colon) {
        snprintf(file, sizeof(file), "%.*s", (int) (colon - name), name);
        name = colon + 1;
    }
    for (size_t i = 0; i < sSymbolCount; ++i) {
        if (sSymbols[i].data && strcmp(sSymbols[i].name, name) == 0 && (!file[0] || strcmp(file, sSymbols[i].file) == 0)) {
            if (size) {
                *size = sSymbols[i].size;
            }
            return (void *) sSymbols[i].address;
        }
    }
    return NULL;
}

const char *elf_symbol_containing(const void *addr, size_t *offset, size_t *size) {
    if (!load_symbols()) {
        return NULL;
    }
    const uintptr_t a = (uintptr_t) addr;
    size_t low = 0, high = sSymbolCount;
    while (low < high) {
        const size_t mid = (low + high) / 2;
        if (sSymbols[mid].address <= a) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    if (low == 0 || a >= sSymbols[low - 1].address + sSymbols[low - 1].size) {
        return NULL;
    }
    *offset = a - sSymbols[low - 1].address;
    *size = sSymbols[low - 1].size;
    return sSymbols[low - 1].name;
}
#endif
