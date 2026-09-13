#ifndef SM64_PHYSICS_SAVE_H
#define SM64_PHYSICS_SAVE_H
#include "game/save_file.h"

struct sm64_save_state;
struct sm64_save_context {
    s16 file_num, course_num, level_num, area_index, act_num, saved_course_num;
    u8 credits, demo;
};
struct sm64_save_result {
    u8 last_course, last_star, file_coin_high_score;
    s8 global_coin_high_score;
};
/* Input is a native, decoded SaveBuffer, not raw big-endian EEPROM bytes.
 * NULL creates empty files using the original initialization routines.
 * Originals validate signatures and repair backups inside the owned copy.
 * There is no disk, ROM or EEPROM I/O. */
struct sm64_save_state *sm64_save_create(const struct SaveBuffer *decoded);
void sm64_save_destroy(struct sm64_save_state *state);
struct sm64_save_state *sm64_save_clone(const struct sm64_save_state *state);
/* All original save calls require an active state and valid upstream indices.
 * A state may be active on only one thread at a time. Setting sound mode also
 * requires an active audio state to emit the host presentation event. */
struct sm64_save_state *sm64_save_activate(struct sm64_save_state *state);
int sm64_save_set_context(struct sm64_save_state *state, struct sm64_save_context context);
struct sm64_save_result sm64_save_get_result(const struct sm64_save_state *state);
/* Copies the committed native image. Unsaved gameplay changes remain in the
 * working copy until save_file_do_save; cloning preserves both copies. */
void sm64_save_export(const struct sm64_save_state *state, struct SaveBuffer *decoded);
#endif
