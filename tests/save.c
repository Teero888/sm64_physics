#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../host/save.h"
#include "../host/audio.h"
#include "level_table.h"

static struct sm64_save_context context = {1, COURSE_BOB, LEVEL_BOB, 1, 1, COURSE_BOB, 0, 0};
static void select_context(struct sm64_save_state *state) {
    assert(sm64_save_set_context(state, context));
    sm64_save_activate(state);
}

int main(void) {
    struct sm64_audio_state *audio = sm64_audio_create(NULL, NULL);
    assert(audio);
    sm64_audio_activate(audio);
    struct sm64_save_state *a = sm64_save_create(NULL);
    struct sm64_save_state *b = sm64_save_create(NULL);
    assert(a && b);
    select_context(a);
    assert(!save_file_exists(0));
    save_file_set_sound_mode(2);
    assert(save_file_get_sound_mode() == 2);
    save_file_collect_star_or_key(100, 0);
    assert(save_file_exists(0));
    assert(save_file_get_star_flags(0, COURSE_NUM_TO_INDEX(COURSE_BOB)) == 1);
    assert(save_file_get_course_coin_score(0, 0) == 100);
    assert(save_file_get_max_coin_score(0) == ((1u << 16) | 100));
    struct sm64_save_result result = sm64_save_get_result(a);
    assert(result.last_course == COURSE_BOB && result.last_star == 1);
    assert(result.file_coin_high_score && result.global_coin_high_score);
    save_file_set_cannon_unlocked();
    assert(save_file_is_cannon_unlocked());
    assert(save_file_get_star_flags(0, COURSE_NUM_TO_INDEX(COURSE_WF)) == 0);
    save_file_set_star_flags(0, COURSE_NUM_TO_INDEX(COURSE_NONE), 3);
    assert(save_file_get_total_star_count(0, 0, COURSE_STAGES_COUNT - 1) == 3);

    struct SaveBuffer image;
    sm64_save_export(a, &image);
    assert(image.files[0][0].flags == 0); // not committed yet
    struct sm64_save_state *clone = sm64_save_clone(a);
    assert(clone);
    save_file_do_save(0);
    sm64_save_export(a, &image);
    assert(image.files[0][0].flags & SAVE_FLAG_FILE_EXISTS);
    assert(memcmp(&image.files[0][0], &image.files[0][1], sizeof(struct SaveFile)) == 0);
    select_context(clone);
    save_file_reload(); // clone retains its own pre-save backup
    assert(!save_file_exists(0));
    select_context(a);
    save_file_set_flags(SAVE_FLAG_HAVE_WING_CAP);
    save_file_reload();
    assert(!(save_file_get_flags() & SAVE_FLAG_HAVE_WING_CAP));
    assert(save_file_is_cannon_unlocked());

    // Preserve the upstream 16-bit comparison / 8-bit score truncation.
    save_file_collect_star_or_key(300, 1);
    assert(save_file_get_course_coin_score(0, 0) == 44);
    assert(save_file_get_star_flags(0, 0) == 3);
    save_file_copy(0, 1);
    assert(save_file_get_star_flags(1, 0) == 3);
    save_file_erase(1);
    assert(!save_file_exists(1) && save_file_exists(0));

    context.level_num = LEVEL_BOWSER_1;
    select_context(a);
    save_file_collect_star_or_key(0, 0);
    assert(save_file_get_flags() & SAVE_FLAG_HAVE_KEY_1);
    save_file_clear_flags(SAVE_FLAG_HAVE_KEY_1);
    save_file_set_flags(SAVE_FLAG_UNLOCKED_BASEMENT_DOOR);
    save_file_collect_star_or_key(0, 0);
    assert(!(save_file_get_flags() & SAVE_FLAG_HAVE_KEY_1));
    context.level_num = LEVEL_SSL;
    select_context(a);
    save_file_set_cap_pos(-100, 250, 300);
    Vec3s cap = {0};
    assert(save_file_get_cap_pos(cap) && cap[0] == -100 && cap[2] == 300);
    context.area_index = 2;
    select_context(a);
    assert(!save_file_get_cap_pos(cap));
    context.area_index = 1;
    select_context(a);
    save_file_move_cap_to_default_location();
    assert(save_file_get_flags() & SAVE_FLAG_CAP_ON_KLEPTO);
    assert(!(save_file_get_flags() & SAVE_FLAG_CAP_ON_GROUND));
    context.demo = 1;
    select_context(a);
    assert(save_file_get_flags() == 0);
    context.demo = 0;
    context.credits = 1;
    select_context(a);
    assert(save_file_get_flags() == 0);
    context.credits = 0;
    select_context(a);
    assert(save_file_get_flags() & SAVE_FLAG_CAP_ON_KLEPTO);

    context.level_num = LEVEL_BOB;
    select_context(a);
    struct WarpNode checkpoint = {.destLevel = LEVEL_BOB | 0x80, .destArea = 2, .destNode = 10};
    check_if_should_set_warp_checkpoint(&checkpoint);
    struct WarpNode destination = {.destLevel = LEVEL_BOB, .destArea = 1, .destNode = 1};
    assert(check_warp_checkpoint(&destination));
    assert(destination.destArea == 2 && destination.destNode == 10);
    context.act_num = 2;
    select_context(a);
    assert(!check_warp_checkpoint(&destination));
    context.act_num = 1;
    select_context(a);
    assert(!check_warp_checkpoint(&destination)); // invalidation persists
    select_context(b);
    assert(!save_file_exists(0));
    assert(!check_warp_checkpoint(&destination));

    // Load from the earlier native image, repairing either missing backup.
    image.files[0][0].signature.magic = 0;
    struct sm64_save_state *restored = sm64_save_create(&image);
    assert(restored);
    select_context(restored);
    assert(save_file_get_star_flags(0, 0) == 1);
    assert(save_file_is_cannon_unlocked());
    sm64_save_export(restored, &image);
    assert(memcmp(&image.files[0][0], &image.files[0][1], sizeof(struct SaveFile)) == 0);
    image.files[0][1].signature.chksum ^= 1;
    image.menuData[0].signature.magic = 0;
    struct sm64_save_state *repaired = sm64_save_create(&image);
    assert(repaired);
    select_context(repaired);
    assert(save_file_get_star_flags(0, 0) == 1);
    sm64_save_export(repaired, &image);
    assert(memcmp(&image.menuData[0], &image.menuData[1], sizeof(struct MainMenuSaveData)) == 0);
    assert(save_file_get_sound_mode() == 2);
    assert(memcmp(&image.files[0][0], &image.files[0][1], sizeof(struct SaveFile)) == 0);
    context.file_num = 0;
    assert(!sm64_save_set_context(a, context));
    sm64_save_destroy(a);
    sm64_save_destroy(b);
    sm64_save_destroy(clone);
    sm64_save_destroy(restored);
    sm64_save_destroy(repaired);
    sm64_audio_destroy(audio);
    puts("Native save progress: stars, keys, cannon, cap, checkpoints, backups and independent continuation passed");
    return 0;
}
