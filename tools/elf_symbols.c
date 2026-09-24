#define _GNU_SOURCE
#include "elf_symbols.h"

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
