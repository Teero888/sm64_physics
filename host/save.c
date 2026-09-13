#include <assert.h>
#include <stdlib.h>
#include "save_state.h"

_Thread_local struct sm64_save_state *sm64_active_save;
_Static_assert(sizeof(struct SaveBuffer) == EEPROM_SIZE, "native save layout size");

struct sm64_save_state *sm64_save_activate(struct sm64_save_state *state) {
    struct sm64_save_state *previous = sm64_active_save;
    sm64_active_save = state;
    return previous;
}

struct sm64_save_state *sm64_save_create(const struct SaveBuffer *decoded) {
    struct sm64_save_state *state = calloc(1, sizeof(*state));
    if (!state) return NULL;
    if (decoded) state->committed = *decoded;
    state->context.file_num = 1;
    state->context.level_num = LEVEL_BOB;
    state->context.area_index = 1;
    state->context.act_num = 1;
    struct sm64_save_state *previous = sm64_save_activate(state);
    save_file_load_all();
    sm64_save_activate(previous);
    return state;
}

void sm64_save_destroy(struct sm64_save_state *state) {
    if (sm64_active_save == state) sm64_active_save = NULL;
    free(state);
}

struct sm64_save_state *sm64_save_clone(const struct sm64_save_state *state) {
    if (!state) return NULL;
    struct sm64_save_state *copy = malloc(sizeof(*copy));
    if (copy) *copy = *state;
    return copy;
}

int sm64_save_set_context(struct sm64_save_state *state, struct sm64_save_context context) {
    if (!state || context.file_num < 1 || context.file_num > NUM_SAVE_FILES ||
        context.course_num < COURSE_NONE || context.course_num > COURSE_MAX ||
        context.saved_course_num < COURSE_NONE || context.saved_course_num > COURSE_MAX ||
        context.level_num < 1 || context.level_num >= LEVEL_COUNT ||
        context.area_index < 1 || context.area_index > 255 ||
        context.act_num < 1 || context.act_num > 255) return 0;
    state->context = context;
    return 1;
}

struct sm64_save_result sm64_save_get_result(const struct sm64_save_state *state) {
    assert(state);
    return state->result;
}

void sm64_save_export(const struct sm64_save_state *state, struct SaveBuffer *decoded) {
    assert(state && decoded);
    *decoded = state->committed;
}

static size_t buffer_offset(void *buffer, s32 size) {
    assert(sm64_active_save && size >= 0);
    size_t offset = (u8 *)buffer - (u8 *)&sm64_active_save->working;
    assert(offset <= sizeof(struct SaveBuffer) && (size_t)size <= sizeof(struct SaveBuffer) - offset);
    return offset;
}

s32 sm64_save_read(void *buffer, s32 size) {
    size_t offset = buffer_offset(buffer, size);
    memcpy(buffer, (u8 *)&sm64_active_save->committed + offset, (size_t)size);
    return 0;
}

s32 sm64_save_write(void *buffer, s32 size) {
    size_t offset = buffer_offset(buffer, size);
    memcpy((u8 *)&sm64_active_save->committed + offset, buffer, (size_t)size);
    return 0;
}
