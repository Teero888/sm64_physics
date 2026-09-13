#ifndef SM64_PHYSICS_SAVE_STATE_H
#define SM64_PHYSICS_SAVE_STATE_H
#include <string.h>
#include "sm64.h"
#include "engine/math_util.h"
#include "game/sound_init.h"
#include "level_table.h"
#include "host/save.h"

struct sm64_save_state {
    struct SaveBuffer working, committed;
    struct sm64_save_context context;
    struct WarpCheckpoint checkpoint;
    s8 menu_modified, file_modified;
    struct sm64_save_result result;
};
extern _Thread_local struct sm64_save_state *sm64_active_save;
s32 sm64_save_read(void *buffer, s32 size);
s32 sm64_save_write(void *buffer, s32 size);
#define read_eeprom_data sm64_save_read
#define write_eeprom_data sm64_save_write
#define bcopy(src, dst, size) memmove((dst), (src), (size))
#define bzero(dst, size) memset((dst), 0, (size))
#define MENU_DATA_MAGIC 0x4849
#define SAVE_FILE_MAGIC 0x4441
#define gSaveBuffer (sm64_active_save->working)
#define gWarpCheckpoint (sm64_active_save->checkpoint)
#define gMainMenuDataModified (sm64_active_save->menu_modified)
#define gSaveFileModified (sm64_active_save->file_modified)
#define gLastCompletedCourseNum (sm64_active_save->result.last_course)
#define gLastCompletedStarNum (sm64_active_save->result.last_star)
#define sUnusedGotGlobalCoinHiScore (sm64_active_save->result.global_coin_high_score)
#define gGotFileCoinHiScore (sm64_active_save->result.file_coin_high_score)
#define gCurrSaveFileNum (sm64_active_save->context.file_num)
#define gCurrCourseNum (sm64_active_save->context.course_num)
#define gCurrLevelNum (sm64_active_save->context.level_num)
#define gCurrAreaIndex (sm64_active_save->context.area_index)
#define gCurrActNum (sm64_active_save->context.act_num)
#define gSavedCourseNum (sm64_active_save->context.saved_course_num)
/* These original routines only test whether credits/demo pointers are NULL. */
#define gCurrCreditsEntry (sm64_active_save->context.credits ? sm64_active_save : NULL)
#define gCurrDemoInput (sm64_active_save->context.demo ? sm64_active_save : NULL)
/* Original macro definitions for the level-to-course table. */
#define STUB_LEVEL(_0, _1, courseenum, _3, _4, _5, _6, _7, _8) courseenum,
#define DEFINE_LEVEL(_0, _1, courseenum, _3, _4, _5, _6, _7, _8, _9, _10) courseenum,
#endif
