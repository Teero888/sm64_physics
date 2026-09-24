// Finds variables of the running executable by name, statics included, from
// its ELF symbol table: "name", or "file.c:name" for a static whose name
// several files use (as the oracle's symbol tables write it).
#pragma once
#include <stddef.h>

void *elf_symbol(const char *name, size_t *size);

// The data or function symbol containing addr, if any: its name, the offset
// of addr in it and its size.
const char *elf_symbol_containing(const void *addr, size_t *offset, size_t *size);
