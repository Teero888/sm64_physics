#include <ultra64.h>

#include "sm64.h"
#include "game_init.h"
#include "main.h"
#include "engine/math_util.h"
#include "area.h"
#include "level_update.h"
#include "save_file.h"
#include "sound_init.h"
#include "level_table.h"
#include "course_table.h"
#include "rumble_init.h"

#define MENU_DATA_MAGIC 0x4849
#define SAVE_FILE_MAGIC 0x4441

STATIC_ASSERT(sizeof(struct SaveBuffer) == EEPROM_SIZE, "eeprom buffer size must match");

extern struct SaveBuffer gSaveBuffer;

struct WarpCheckpoint gWarpCheckpoint;

s8 gMainMenuDataModified;
s8 gSaveFileModified;

u8 gLastCompletedCourseNum = COURSE_NONE;
u8 gLastCompletedStarNum = 0;
s8 sUnusedGotGlobalCoinHiScore = FALSE;
u8 gGotFileCoinHiScore = FALSE;
u8 gCurrCourseStarFlags = 0;

u8 gSpecialTripleJump = FALSE;

#define STUB_LEVEL(_0, _1, courseenum, _3, _4, _5, _6, _7, _8) courseenum,
#define DEFINE_LEVEL(_0, _1, courseenum, _3, _4, _5, _6, _7, _8, _9, _10) courseenum,

s8 gLevelToCourseNumTable[] = {
    #include "levels/level_defines.h"
};
#undef STUB_LEVEL
#undef DEFINE_LEVEL

STATIC_ASSERT(ARRAY_COUNT(gLevelToCourseNumTable) == LEVEL_COUNT - 1,
              "change this array if you are adding levels");

// This was probably used to set progress to 100% for debugging, but
// it was removed from the release ROM.
static void stub_save_file_1(void) {
    UNUSED u8 filler[4];
}

/**
 * Read from EEPROM to a given address.
 * The EEPROM address is computed using the offset of the destination address from gSaveBuffer.
 * Try at most 4 times, and return 0 on success. On failure, return the status returned from
 * osEepromLongRead. It also returns 0 if EEPROM isn't loaded correctly in the system.
 */
static s32 read_eeprom_data(void *buffer, s32 size) {
    s32 status = 0;

    if (WORLD(gEepromProbe) != 0) {
        s32 triesLeft = 4;
        u32 offset = (u32)((u8 *) buffer - (u8 *) &WORLD(gSaveBuffer)) / 8;

        do {
#if ENABLE_RUMBLE
            block_until_rumble_pak_free();
#endif
            triesLeft--;
            status = osEepromLongRead(&WORLD(gSIEventMesgQueue), offset, buffer, size);
#if ENABLE_RUMBLE
            release_rumble_pak_control();
#endif
        } while (triesLeft > 0 && status != 0);
    }

    return status;
}

/**
 * Write data to EEPROM.
 * The EEPROM address is computed using the offset of the source address from gSaveBuffer.
 * Try at most 4 times, and return 0 on success. On failure, return the status returned from
 * osEepromLongWrite. Unlike read_eeprom_data, return 1 if EEPROM isn't loaded.
 */
static s32 write_eeprom_data(void *buffer, s32 size) {
    s32 status = 1;

    if (WORLD(gEepromProbe) != 0) {
        s32 triesLeft = 4;
        u32 offset = (u32)((u8 *) buffer - (u8 *) &WORLD(gSaveBuffer)) >> 3;

        do {
#if ENABLE_RUMBLE
            block_until_rumble_pak_free();
#endif
            triesLeft--;
            status = osEepromLongWrite(&WORLD(gSIEventMesgQueue), offset, buffer, size);
#if ENABLE_RUMBLE
            release_rumble_pak_control();
#endif
        } while (triesLeft > 0 && status != 0);
    }

    return status;
}

/**
 * Sum the bytes in data to data + size - 2. The last two bytes are ignored
 * because that is where the checksum is stored.
 */
static u16 calc_checksum(u8 *data, s32 size) {
    u16 chksum = 0;

    while (size-- > 2) {
        chksum += *data++;
    }
    return chksum;
}

/**
 * Verify the signature at the end of the block to check if the data is valid.
 */
static s32 verify_save_block_signature(void *buffer, s32 size, u16 magic) {
    struct SaveBlockSignature *sig = (struct SaveBlockSignature *) ((size - 4) + (u8 *) buffer);

    if (sig->magic != magic) {
        return FALSE;
    }
    if (sig->chksum != calc_checksum(buffer, size)) {
        return FALSE;
    }
    return TRUE;
}

/**
 * Write a signature at the end of the block to make sure the data is valid
 */
static void add_save_block_signature(void *buffer, s32 size, u16 magic) {
    struct SaveBlockSignature *sig = (struct SaveBlockSignature *) ((size - 4) + (u8 *) buffer);

    sig->magic = magic;
    sig->chksum = calc_checksum(buffer, size);
}

/**
 * Copy main menu data from one backup slot to the other slot.
 */
static void restore_main_menu_data(s32 srcSlot) {
    s32 destSlot = srcSlot ^ 1;

    // Compute checksum on source data
    add_save_block_signature(&WORLD(gSaveBuffer).menuData[srcSlot], sizeof(WORLD(gSaveBuffer).menuData[srcSlot]), MENU_DATA_MAGIC);

    // Copy source data to destination
    bcopy(&WORLD(gSaveBuffer).menuData[srcSlot], &WORLD(gSaveBuffer).menuData[destSlot], sizeof(WORLD(gSaveBuffer).menuData[destSlot]));

    // Write destination data to EEPROM
    write_eeprom_data(&WORLD(gSaveBuffer).menuData[destSlot], sizeof(WORLD(gSaveBuffer).menuData[destSlot]));
}

static void save_main_menu_data(void) {
    if (WORLD(gMainMenuDataModified)) {
        // Compute checksum
        add_save_block_signature(&WORLD(gSaveBuffer).menuData[0], sizeof(WORLD(gSaveBuffer).menuData[0]), MENU_DATA_MAGIC);

        // Back up data
        bcopy(&WORLD(gSaveBuffer).menuData[0], &WORLD(gSaveBuffer).menuData[1], sizeof(WORLD(gSaveBuffer).menuData[1]));

        // Write to EEPROM
        write_eeprom_data(WORLD(gSaveBuffer).menuData, sizeof(WORLD(gSaveBuffer).menuData));

        WORLD(gMainMenuDataModified) = FALSE;
    }
}

static void wipe_main_menu_data(void) {
    bzero(&WORLD(gSaveBuffer).menuData[0], sizeof(WORLD(gSaveBuffer).menuData[0]));

    // Set score ages for all courses to 3, 2, 1, and 0, respectively.
    WORLD(gSaveBuffer).menuData[0].coinScoreAges[0] = 0x3FFFFFFF;
    WORLD(gSaveBuffer).menuData[0].coinScoreAges[1] = 0x2AAAAAAA;
    WORLD(gSaveBuffer).menuData[0].coinScoreAges[2] = 0x15555555;

    WORLD(gMainMenuDataModified) = TRUE;
    save_main_menu_data();
}

static s32 get_coin_score_age(s32 fileIndex, s32 courseIndex) {
    return (WORLD(gSaveBuffer).menuData[0].coinScoreAges[fileIndex] >> (2 * courseIndex)) & 0x3;
}

static void set_coin_score_age(s32 fileIndex, s32 courseIndex, s32 age) {
    s32 mask = 0x3 << (2 * courseIndex);

    WORLD(gSaveBuffer).menuData[0].coinScoreAges[fileIndex] &= ~mask;
    WORLD(gSaveBuffer).menuData[0].coinScoreAges[fileIndex] |= age << (2 * courseIndex);
}

/**
 * Mark a coin score for a save file as the newest out of all save files.
 */
static void touch_coin_score_age(s32 fileIndex, s32 courseIndex) {
    s32 i;
    u32 age;
    u32 currentAge = get_coin_score_age(fileIndex, courseIndex);

    if (currentAge != 0) {
        for (i = 0; i < NUM_SAVE_FILES; i++) {
            age = get_coin_score_age(i, courseIndex);
            if (age < currentAge) {
                set_coin_score_age(i, courseIndex, age + 1);
            }
        }

        set_coin_score_age(fileIndex, courseIndex, 0);
        WORLD(gMainMenuDataModified) = TRUE;
    }
}

/**
 * Mark all coin scores for a save file as new.
 */
static void touch_high_score_ages(s32 fileIndex) {
    s32 i;

    for (i = COURSE_NUM_TO_INDEX(COURSE_MIN); i <= COURSE_NUM_TO_INDEX(COURSE_STAGES_MAX); i++) {
        touch_coin_score_age(fileIndex, i);
    }
}

/**
 * Copy save file data from one backup slot to the other slot.
 */
static void restore_save_file_data(s32 fileIndex, s32 srcSlot) {
    s32 destSlot = srcSlot ^ 1;

    // Compute checksum on source data
    add_save_block_signature(&WORLD(gSaveBuffer).files[fileIndex][srcSlot],
                             sizeof(WORLD(gSaveBuffer).files[fileIndex][srcSlot]), SAVE_FILE_MAGIC);

    // Copy source data to destination slot
    bcopy(&WORLD(gSaveBuffer).files[fileIndex][srcSlot], &WORLD(gSaveBuffer).files[fileIndex][destSlot],
          sizeof(WORLD(gSaveBuffer).files[fileIndex][destSlot]));

    // Write destination data to EEPROM
    write_eeprom_data(&WORLD(gSaveBuffer).files[fileIndex][destSlot],
                      sizeof(WORLD(gSaveBuffer).files[fileIndex][destSlot]));
}

void save_file_do_save(s32 fileIndex) {
    if (WORLD(gSaveFileModified)) {
        // Compute checksum
        add_save_block_signature(&WORLD(gSaveBuffer).files[fileIndex][0],
                                 sizeof(WORLD(gSaveBuffer).files[fileIndex][0]), SAVE_FILE_MAGIC);

        // Copy to backup slot
        bcopy(&WORLD(gSaveBuffer).files[fileIndex][0], &WORLD(gSaveBuffer).files[fileIndex][1],
              sizeof(WORLD(gSaveBuffer).files[fileIndex][1]));

        // Write to EEPROM
        write_eeprom_data(WORLD(gSaveBuffer).files[fileIndex], sizeof(WORLD(gSaveBuffer).files[fileIndex]));

        WORLD(gSaveFileModified) = FALSE;
    }

    save_main_menu_data();
}

void save_file_erase(s32 fileIndex) {
    touch_high_score_ages(fileIndex);
    bzero(&WORLD(gSaveBuffer).files[fileIndex][0], sizeof(WORLD(gSaveBuffer).files[fileIndex][0]));

    WORLD(gSaveFileModified) = TRUE;
    save_file_do_save(fileIndex);
}

//! Needs to be s32 to match on -O2, despite no return value.
BAD_RETURN(s32) save_file_copy(s32 srcFileIndex, s32 destFileIndex) {
    UNUSED u8 filler[4];

    touch_high_score_ages(destFileIndex);
    bcopy(&WORLD(gSaveBuffer).files[srcFileIndex][0], &WORLD(gSaveBuffer).files[destFileIndex][0],
          sizeof(WORLD(gSaveBuffer).files[destFileIndex][0]));

    WORLD(gSaveFileModified) = TRUE;
    save_file_do_save(destFileIndex);
}

void save_file_load_all(void) {
    s32 file;
    s32 validSlots;

    WORLD(gMainMenuDataModified) = FALSE;
    WORLD(gSaveFileModified) = FALSE;

    bzero(&WORLD(gSaveBuffer), sizeof(WORLD(gSaveBuffer)));
    read_eeprom_data(&WORLD(gSaveBuffer), sizeof(WORLD(gSaveBuffer)));

    // Verify the main menu data and create a backup copy if only one of the slots is valid.
    validSlots = verify_save_block_signature(&WORLD(gSaveBuffer).menuData[0], sizeof(WORLD(gSaveBuffer).menuData[0]), MENU_DATA_MAGIC);
    validSlots |= verify_save_block_signature(&WORLD(gSaveBuffer).menuData[1], sizeof(WORLD(gSaveBuffer).menuData[1]),MENU_DATA_MAGIC) << 1;
    switch (validSlots) {
        case 0: // Neither copy is correct
            wipe_main_menu_data();
            break;
        case 1: // Slot 0 is correct and slot 1 is incorrect
            restore_main_menu_data(0);
            break;
        case 2: // Slot 1 is correct and slot 0 is incorrect
            restore_main_menu_data(1);
            break;
    }

    for (file = 0; file < NUM_SAVE_FILES; file++) {
        // Verify the save file and create a backup copy if only one of the slots is valid.
        validSlots = verify_save_block_signature(&WORLD(gSaveBuffer).files[file][0], sizeof(WORLD(gSaveBuffer).files[file][0]), SAVE_FILE_MAGIC);
        validSlots |= verify_save_block_signature(&WORLD(gSaveBuffer).files[file][1], sizeof(WORLD(gSaveBuffer).files[file][1]), SAVE_FILE_MAGIC) << 1;
        switch (validSlots) {
            case 0: // Neither copy is correct
                save_file_erase(file);
                break;
            case 1: // Slot 0 is correct and slot 1 is incorrect
                restore_save_file_data(file, 0);
                break;
            case 2: // Slot 1 is correct and slot 0 is incorrect
                restore_save_file_data(file, 1);
                break;
        }
    }

    stub_save_file_1();
}

/**
 * Reload the current save file from its backup copy, which is effectively a
 * a cached copy of what has been written to EEPROM.
 * This is used after getting a game over.
 */
void save_file_reload(void) {
    // Copy save file data from backup
    bcopy(&WORLD(gSaveBuffer).files[WORLD(gCurrSaveFileNum) - 1][1], &WORLD(gSaveBuffer).files[WORLD(gCurrSaveFileNum) - 1][0],
          sizeof(WORLD(gSaveBuffer).files[WORLD(gCurrSaveFileNum) - 1][0]));

    // Copy main menu data from backup
    bcopy(&WORLD(gSaveBuffer).menuData[1], &WORLD(gSaveBuffer).menuData[0], sizeof(WORLD(gSaveBuffer).menuData[0]));

    WORLD(gMainMenuDataModified) = FALSE;
    WORLD(gSaveFileModified) = FALSE;
}

/**
 * Update the current save file after collecting a star or a key.
 * If coin score is greater than the current high score, update it.
 */
void save_file_collect_star_or_key(s16 coinScore, s16 starIndex) {
    s32 fileIndex = WORLD(gCurrSaveFileNum) - 1;
    s32 courseIndex = COURSE_NUM_TO_INDEX(WORLD(gCurrCourseNum));

    s32 starFlag = 1 << starIndex;
    UNUSED s32 flags = save_file_get_flags();

    WORLD(gLastCompletedCourseNum) = courseIndex + 1;
    WORLD(gLastCompletedStarNum) = starIndex + 1;
    WORLD(sUnusedGotGlobalCoinHiScore) = FALSE;
    WORLD(gGotFileCoinHiScore) = FALSE;

    if (courseIndex >= COURSE_NUM_TO_INDEX(COURSE_MIN)
        && courseIndex <= COURSE_NUM_TO_INDEX(COURSE_STAGES_MAX)) {
        //! Compares the coin score as a 16 bit value, but only writes the 8 bit
        // truncation. This can allow a high score to decrease.

        if (coinScore > ((u16) save_file_get_max_coin_score(courseIndex) & 0xFFFF)) {
            WORLD(sUnusedGotGlobalCoinHiScore) = TRUE;
        }

        if (coinScore > save_file_get_course_coin_score(fileIndex, courseIndex)) {
            WORLD(gSaveBuffer).files[fileIndex][0].courseCoinScores[courseIndex] = coinScore;
            touch_coin_score_age(fileIndex, courseIndex);

            WORLD(gGotFileCoinHiScore) = TRUE;
            WORLD(gSaveFileModified) = TRUE;
        }
    }

    switch (WORLD(gCurrLevelNum)) {
        case LEVEL_BOWSER_1:
            if (!(save_file_get_flags() & (SAVE_FLAG_HAVE_KEY_1 | SAVE_FLAG_UNLOCKED_BASEMENT_DOOR))) {
                save_file_set_flags(SAVE_FLAG_HAVE_KEY_1);
            }
            break;

        case LEVEL_BOWSER_2:
            if (!(save_file_get_flags() & (SAVE_FLAG_HAVE_KEY_2 | SAVE_FLAG_UNLOCKED_UPSTAIRS_DOOR))) {
                save_file_set_flags(SAVE_FLAG_HAVE_KEY_2);
            }
            break;

        case LEVEL_BOWSER_3:
            break;

        default:
            if (!(save_file_get_star_flags(fileIndex, courseIndex) & starFlag)) {
                save_file_set_star_flags(fileIndex, courseIndex, starFlag);
            }
            break;
    }
}

s32 save_file_exists(s32 fileIndex) {
    return (WORLD(gSaveBuffer).files[fileIndex][0].flags & SAVE_FLAG_FILE_EXISTS) != 0;
}

/**
 * Get the maximum coin score across all files for a course. The lower 16 bits
 * of the returned value are the score, and the upper 16 bits are the file number
 * of the save file with this score.
 */
u32 save_file_get_max_coin_score(s32 courseIndex) {
    s32 fileIndex;
    s32 maxCoinScore = -1;
    s32 maxScoreAge = -1;
    s32 maxScoreFileNum = 0;

    for (fileIndex = 0; fileIndex < NUM_SAVE_FILES; fileIndex++) {
        if (save_file_get_star_flags(fileIndex, courseIndex) != 0) {
            s32 coinScore = save_file_get_course_coin_score(fileIndex, courseIndex);
            s32 scoreAge = get_coin_score_age(fileIndex, courseIndex);

            if (coinScore > maxCoinScore || (coinScore == maxCoinScore && scoreAge > maxScoreAge)) {
                maxCoinScore = coinScore;
                maxScoreAge = scoreAge;
                maxScoreFileNum = fileIndex + 1;
            }
        }
    }
    return (maxScoreFileNum << 16) + max(maxCoinScore, 0);
}

s32 save_file_get_course_star_count(s32 fileIndex, s32 courseIndex) {
    s32 i;
    s32 count = 0;
    u8 flag = 1;
    u8 starFlags = save_file_get_star_flags(fileIndex, courseIndex);

    for (i = 0; i < 7; i++, flag <<= 1) {
        if (starFlags & flag) {
            count++;
        }
    }
    return count;
}

s32 save_file_get_total_star_count(s32 fileIndex, s32 minCourse, s32 maxCourse) {
    s32 count = 0;

    // Get standard course star count.
    for (; minCourse <= maxCourse; minCourse++) {
        count += save_file_get_course_star_count(fileIndex, minCourse);
    }

    // Add castle secret star count.
    return save_file_get_course_star_count(fileIndex, COURSE_NUM_TO_INDEX(COURSE_NONE)) + count;
}

void save_file_set_flags(u32 flags) {
    WORLD(gSaveBuffer).files[WORLD(gCurrSaveFileNum) - 1][0].flags |= (flags | SAVE_FLAG_FILE_EXISTS);
    WORLD(gSaveFileModified) = TRUE;
}

void save_file_clear_flags(u32 flags) {
    WORLD(gSaveBuffer).files[WORLD(gCurrSaveFileNum) - 1][0].flags &= ~flags;
    WORLD(gSaveBuffer).files[WORLD(gCurrSaveFileNum) - 1][0].flags |= SAVE_FLAG_FILE_EXISTS;
    WORLD(gSaveFileModified) = TRUE;
}

u32 save_file_get_flags(void) {
    if (WORLD(gCurrCreditsEntry) != NULL || WORLD(gCurrDemoInput) != NULL) {
        return 0;
    }
    return WORLD(gSaveBuffer).files[WORLD(gCurrSaveFileNum) - 1][0].flags;
}

/**
 * Return the bitset of obtained stars in the specified course.
 * If course is COURSE_NONE, return the bitset of obtained castle secret stars.
 */
u32 save_file_get_star_flags(s32 fileIndex, s32 courseIndex) {
    u32 starFlags;

    if (courseIndex == COURSE_NUM_TO_INDEX(COURSE_NONE)) {
        starFlags = SAVE_FLAG_TO_STAR_FLAG(WORLD(gSaveBuffer).files[fileIndex][0].flags);
    } else {
        starFlags = WORLD(gSaveBuffer).files[fileIndex][0].courseStars[courseIndex] & 0x7F;
    }

    return starFlags;
}

/**
 * Add to the bitset of obtained stars in the specified course.
 * If course is COURSE_NONE, add to the bitset of obtained castle secret stars.
 */
void save_file_set_star_flags(s32 fileIndex, s32 courseIndex, u32 starFlags) {
    if (courseIndex == COURSE_NUM_TO_INDEX(COURSE_NONE)) {
        WORLD(gSaveBuffer).files[fileIndex][0].flags |= STAR_FLAG_TO_SAVE_FLAG(starFlags);
    } else {
        WORLD(gSaveBuffer).files[fileIndex][0].courseStars[courseIndex] |= starFlags;
    }

    WORLD(gSaveBuffer).files[fileIndex][0].flags |= SAVE_FLAG_FILE_EXISTS;
    WORLD(gSaveFileModified) = TRUE;
}

s32 save_file_get_course_coin_score(s32 fileIndex, s32 courseIndex) {
    return WORLD(gSaveBuffer).files[fileIndex][0].courseCoinScores[courseIndex];
}

/**
 * Return TRUE if the cannon is unlocked in the current course.
 */
s32 save_file_is_cannon_unlocked(void) {
    return (WORLD(gSaveBuffer).files[WORLD(gCurrSaveFileNum) - 1][0].courseStars[WORLD(gCurrCourseNum)] & (1 << 7)) != 0;
}

/**
 * Sets the cannon status to unlocked in the current course.
 */
void save_file_set_cannon_unlocked(void) {
    WORLD(gSaveBuffer).files[WORLD(gCurrSaveFileNum) - 1][0].courseStars[WORLD(gCurrCourseNum)] |= (1 << 7);
    WORLD(gSaveBuffer).files[WORLD(gCurrSaveFileNum) - 1][0].flags |= SAVE_FLAG_FILE_EXISTS;
    WORLD(gSaveFileModified) = TRUE;
}

void save_file_set_cap_pos(s16 x, s16 y, s16 z) {
    struct SaveFile *saveFile = &WORLD(gSaveBuffer).files[WORLD(gCurrSaveFileNum) - 1][0];

    saveFile->capLevel = WORLD(gCurrLevelNum);
    saveFile->capArea = WORLD(gCurrAreaIndex);
    vec3s_set(saveFile->capPos, x, y, z);
    save_file_set_flags(SAVE_FLAG_CAP_ON_GROUND);
}

s32 save_file_get_cap_pos(Vec3s capPos) {
    struct SaveFile *saveFile = &WORLD(gSaveBuffer).files[WORLD(gCurrSaveFileNum) - 1][0];
    s32 flags = save_file_get_flags();

    if (saveFile->capLevel == WORLD(gCurrLevelNum) && saveFile->capArea == WORLD(gCurrAreaIndex)
        && (flags & SAVE_FLAG_CAP_ON_GROUND)) {
        vec3s_copy(capPos, saveFile->capPos);
        return TRUE;
    }
    return FALSE;
}

void save_file_set_sound_mode(u16 mode) {
    set_sound_mode(mode);
    WORLD(gSaveBuffer).menuData[0].soundMode = mode;

    WORLD(gMainMenuDataModified) = TRUE;
    save_main_menu_data();
}

u16 save_file_get_sound_mode(void) {
    return WORLD(gSaveBuffer).menuData[0].soundMode;
}

void save_file_move_cap_to_default_location(void) {
    if (save_file_get_flags() & SAVE_FLAG_CAP_ON_GROUND) {
        switch (WORLD(gSaveBuffer).files[WORLD(gCurrSaveFileNum) - 1][0].capLevel) {
            case LEVEL_SSL:
                save_file_set_flags(SAVE_FLAG_CAP_ON_KLEPTO);
                break;
            case LEVEL_SL:
                save_file_set_flags(SAVE_FLAG_CAP_ON_MR_BLIZZARD);
                break;
            case LEVEL_TTM:
                save_file_set_flags(SAVE_FLAG_CAP_ON_UKIKI);
                break;
        }
        save_file_clear_flags(SAVE_FLAG_CAP_ON_GROUND);
    }
}

#ifdef VERSION_EU
void eu_set_language(u16 language) {
    WORLD(gSaveBuffer).menuData[0].language = language;
    WORLD(gMainMenuDataModified) = TRUE;
    save_main_menu_data();
}

u16 eu_get_language(void) {
    return WORLD(gSaveBuffer).menuData[0].language;
}
#endif

void disable_warp_checkpoint(void) {
    // check_warp_checkpoint() checks to see if gWarpCheckpoint.courseNum != COURSE_NONE
    WORLD(gWarpCheckpoint).courseNum = COURSE_NONE;
}

/**
 * Checks the upper bit of the WarpNode->destLevel byte to see if the
 * game should set a warp checkpoint.
 */
void check_if_should_set_warp_checkpoint(struct WarpNode *warpNode) {
    if (warpNode->destLevel & 0x80) {
        // Overwrite the warp checkpoint variables.
        WORLD(gWarpCheckpoint).actNum = WORLD(gCurrActNum);
        WORLD(gWarpCheckpoint).courseNum = WORLD(gCurrCourseNum);
        WORLD(gWarpCheckpoint).levelID = warpNode->destLevel & 0x7F;
        WORLD(gWarpCheckpoint).areaNum = warpNode->destArea;
        WORLD(gWarpCheckpoint).warpNode = warpNode->destNode;
    }
}

/**
 * Checks to see if a checkpoint is properly active or not. This will
 * also update the level, area, and destination node of the input WarpNode.
 * returns TRUE if input WarpNode was updated, and FALSE if not.
 */
s32 check_warp_checkpoint(struct WarpNode *warpNode) {
    s16 warpCheckpointActive = FALSE;
    s16 currCourseNum = WORLD(gLevelToCourseNumTable)[(warpNode->destLevel & 0x7F) - 1];

    // gSavedCourseNum is only used in this function.
    if (WORLD(gWarpCheckpoint).courseNum != COURSE_NONE && WORLD(gSavedCourseNum) == currCourseNum
        && WORLD(gWarpCheckpoint).actNum == WORLD(gCurrActNum)) {
        warpNode->destLevel = WORLD(gWarpCheckpoint).levelID;
        warpNode->destArea = WORLD(gWarpCheckpoint).areaNum;
        warpNode->destNode = WORLD(gWarpCheckpoint).warpNode;
        warpCheckpointActive = TRUE;
    } else {
        // Disable the warp checkpoint just in case the other 2 conditions failed?
        WORLD(gWarpCheckpoint).courseNum = COURSE_NONE;
    }

    return warpCheckpointActive;
}

// Library: its variables' addresses (tools/state/types.py).
#include "pointers/game/src/game/save_file.c.inc.c"
