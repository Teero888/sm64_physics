// A leaf member of a struct, laid out on the N64 and natively
// (generated into layout_tables.h by tools/layout/layout.py).
#pragma once
#include <stddef.h>
#include <stdint.h>

enum leaf_kind { LEAF_S8, LEAF_U8, LEAF_S16, LEAF_U16, LEAF_S32, LEAF_U32, LEAF_S64, LEAF_U64, LEAF_F32, LEAF_F64, LEAF_PTR };

struct leaf {
    const char *path;
    uint32_t n64_offset, native_offset;
    int kind;
    uint32_t count, n64_stride, native_stride;
    const char *target; // pointed-to type, for pointers
};
