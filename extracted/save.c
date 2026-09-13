/* Generated verbatim upstream function extraction. See extracted.json. */
#include "host/save_state.h"

#line 38 "n64decomp/src/game/save_file.c"
s8 gLevelToCourseNumTable[] = {
    #include "levels/level_defines.h"
};

#line 49 "n64decomp/src/game/save_file.c"
static void stub_save_file_1(void) {
    UNUSED u8 filler[4];
}

#line 113 "n64decomp/src/game/save_file.c"
static u16 calc_checksum(u8 *data, s32 size) {
    u16 chksum = 0;

    while (size-- > 2) {
        chksum += *data++;
    }
    return chksum;
}

#line 125 "n64decomp/src/game/save_file.c"
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

#line 140 "n64decomp/src/game/save_file.c"
static void add_save_block_signature(void *buffer, s32 size, u16 magic) {
    struct SaveBlockSignature *sig = (struct SaveBlockSignature *) ((size - 4) + (u8 *) buffer);

    sig->magic = magic;
    sig->chksum = calc_checksum(buffer, size);
}

#line 150 "n64decomp/src/game/save_file.c"
static void restore_main_menu_data(s32 srcSlot) {
    s32 destSlot = srcSlot ^ 1;

    // Compute checksum on source data
    add_save_block_signature(&gSaveBuffer.menuData[srcSlot], sizeof(gSaveBuffer.menuData[srcSlot]), MENU_DATA_MAGIC);

    // Copy source data to destination
    bcopy(&gSaveBuffer.menuData[srcSlot], &gSaveBuffer.menuData[destSlot], sizeof(gSaveBuffer.menuData[destSlot]));

    // Write destination data to EEPROM
    write_eeprom_data(&gSaveBuffer.menuData[destSlot], sizeof(gSaveBuffer.menuData[destSlot]));
}

#line 163 "n64decomp/src/game/save_file.c"
static void save_main_menu_data(void) {
    if (gMainMenuDataModified) {
        // Compute checksum
        add_save_block_signature(&gSaveBuffer.menuData[0], sizeof(gSaveBuffer.menuData[0]), MENU_DATA_MAGIC);

        // Back up data
        bcopy(&gSaveBuffer.menuData[0], &gSaveBuffer.menuData[1], sizeof(gSaveBuffer.menuData[1]));

        // Write to EEPROM
        write_eeprom_data(gSaveBuffer.menuData, sizeof(gSaveBuffer.menuData));

        gMainMenuDataModified = FALSE;
    }
}

#line 178 "n64decomp/src/game/save_file.c"
static void wipe_main_menu_data(void) {
    bzero(&gSaveBuffer.menuData[0], sizeof(gSaveBuffer.menuData[0]));

    // Set score ages for all courses to 3, 2, 1, and 0, respectively.
    gSaveBuffer.menuData[0].coinScoreAges[0] = 0x3FFFFFFF;
    gSaveBuffer.menuData[0].coinScoreAges[1] = 0x2AAAAAAA;
    gSaveBuffer.menuData[0].coinScoreAges[2] = 0x15555555;

    gMainMenuDataModified = TRUE;
    save_main_menu_data();
}

#line 190 "n64decomp/src/game/save_file.c"
static s32 get_coin_score_age(s32 fileIndex, s32 courseIndex) {
    return (gSaveBuffer.menuData[0].coinScoreAges[fileIndex] >> (2 * courseIndex)) & 0x3;
}

#line 194 "n64decomp/src/game/save_file.c"
static void set_coin_score_age(s32 fileIndex, s32 courseIndex, s32 age) {
    s32 mask = 0x3 << (2 * courseIndex);

    gSaveBuffer.menuData[0].coinScoreAges[fileIndex] &= ~mask;
    gSaveBuffer.menuData[0].coinScoreAges[fileIndex] |= age << (2 * courseIndex);
}

#line 204 "n64decomp/src/game/save_file.c"
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
        gMainMenuDataModified = TRUE;
    }
}

#line 225 "n64decomp/src/game/save_file.c"
static void touch_high_score_ages(s32 fileIndex) {
    s32 i;

    for (i = COURSE_NUM_TO_INDEX(COURSE_MIN); i <= COURSE_NUM_TO_INDEX(COURSE_STAGES_MAX); i++) {
        touch_coin_score_age(fileIndex, i);
    }
}

#line 236 "n64decomp/src/game/save_file.c"
static void restore_save_file_data(s32 fileIndex, s32 srcSlot) {
    s32 destSlot = srcSlot ^ 1;

    // Compute checksum on source data
    add_save_block_signature(&gSaveBuffer.files[fileIndex][srcSlot],
                             sizeof(gSaveBuffer.files[fileIndex][srcSlot]), SAVE_FILE_MAGIC);

    // Copy source data to destination slot
    bcopy(&gSaveBuffer.files[fileIndex][srcSlot], &gSaveBuffer.files[fileIndex][destSlot],
          sizeof(gSaveBuffer.files[fileIndex][destSlot]));

    // Write destination data to EEPROM
    write_eeprom_data(&gSaveBuffer.files[fileIndex][destSlot],
                      sizeof(gSaveBuffer.files[fileIndex][destSlot]));
}

#line 252 "n64decomp/src/game/save_file.c"
void save_file_do_save(s32 fileIndex) {
    if (gSaveFileModified) {
        // Compute checksum
        add_save_block_signature(&gSaveBuffer.files[fileIndex][0],
                                 sizeof(gSaveBuffer.files[fileIndex][0]), SAVE_FILE_MAGIC);

        // Copy to backup slot
        bcopy(&gSaveBuffer.files[fileIndex][0], &gSaveBuffer.files[fileIndex][1],
              sizeof(gSaveBuffer.files[fileIndex][1]));

        // Write to EEPROM
        write_eeprom_data(gSaveBuffer.files[fileIndex], sizeof(gSaveBuffer.files[fileIndex]));

        gSaveFileModified = FALSE;
    }

    save_main_menu_data();
}

#line 271 "n64decomp/src/game/save_file.c"
void save_file_erase(s32 fileIndex) {
    touch_high_score_ages(fileIndex);
    bzero(&gSaveBuffer.files[fileIndex][0], sizeof(gSaveBuffer.files[fileIndex][0]));

    gSaveFileModified = TRUE;
    save_file_do_save(fileIndex);
}

#line 280 "n64decomp/src/game/save_file.c"
BAD_RETURN(s32) save_file_copy(s32 srcFileIndex, s32 destFileIndex) {
    UNUSED u8 filler[4];

    touch_high_score_ages(destFileIndex);
    bcopy(&gSaveBuffer.files[srcFileIndex][0], &gSaveBuffer.files[destFileIndex][0],
          sizeof(gSaveBuffer.files[destFileIndex][0]));

    gSaveFileModified = TRUE;
    save_file_do_save(destFileIndex);
}

#line 291 "n64decomp/src/game/save_file.c"
void save_file_load_all(void) {
    s32 file;
    s32 validSlots;

    gMainMenuDataModified = FALSE;
    gSaveFileModified = FALSE;

    bzero(&gSaveBuffer, sizeof(gSaveBuffer));
    read_eeprom_data(&gSaveBuffer, sizeof(gSaveBuffer));

    // Verify the main menu data and create a backup copy if only one of the slots is valid.
    validSlots = verify_save_block_signature(&gSaveBuffer.menuData[0], sizeof(gSaveBuffer.menuData[0]), MENU_DATA_MAGIC);
    validSlots |= verify_save_block_signature(&gSaveBuffer.menuData[1], sizeof(gSaveBuffer.menuData[1]),MENU_DATA_MAGIC) << 1;
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
        validSlots = verify_save_block_signature(&gSaveBuffer.files[file][0], sizeof(gSaveBuffer.files[file][0]), SAVE_FILE_MAGIC);
        validSlots |= verify_save_block_signature(&gSaveBuffer.files[file][1], sizeof(gSaveBuffer.files[file][1]), SAVE_FILE_MAGIC) << 1;
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

#line 341 "n64decomp/src/game/save_file.c"
void save_file_reload(void) {
    // Copy save file data from backup
    bcopy(&gSaveBuffer.files[gCurrSaveFileNum - 1][1], &gSaveBuffer.files[gCurrSaveFileNum - 1][0],
          sizeof(gSaveBuffer.files[gCurrSaveFileNum - 1][0]));

    // Copy main menu data from backup
    bcopy(&gSaveBuffer.menuData[1], &gSaveBuffer.menuData[0], sizeof(gSaveBuffer.menuData[0]));

    gMainMenuDataModified = FALSE;
    gSaveFileModified = FALSE;
}

#line 357 "n64decomp/src/game/save_file.c"
void save_file_collect_star_or_key(s16 coinScore, s16 starIndex) {
    s32 fileIndex = gCurrSaveFileNum - 1;
    s32 courseIndex = COURSE_NUM_TO_INDEX(gCurrCourseNum);

    s32 starFlag = 1 << starIndex;
    UNUSED s32 flags = save_file_get_flags();

    gLastCompletedCourseNum = courseIndex + 1;
    gLastCompletedStarNum = starIndex + 1;
    sUnusedGotGlobalCoinHiScore = FALSE;
    gGotFileCoinHiScore = FALSE;

    if (courseIndex >= COURSE_NUM_TO_INDEX(COURSE_MIN)
        && courseIndex <= COURSE_NUM_TO_INDEX(COURSE_STAGES_MAX)) {
        //! Compares the coin score as a 16 bit value, but only writes the 8 bit
        // truncation. This can allow a high score to decrease.

        if (coinScore > ((u16) save_file_get_max_coin_score(courseIndex) & 0xFFFF)) {
            sUnusedGotGlobalCoinHiScore = TRUE;
        }

        if (coinScore > save_file_get_course_coin_score(fileIndex, courseIndex)) {
            gSaveBuffer.files[fileIndex][0].courseCoinScores[courseIndex] = coinScore;
            touch_coin_score_age(fileIndex, courseIndex);

            gGotFileCoinHiScore = TRUE;
            gSaveFileModified = TRUE;
        }
    }

    switch (gCurrLevelNum) {
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

#line 411 "n64decomp/src/game/save_file.c"
s32 save_file_exists(s32 fileIndex) {
    return (gSaveBuffer.files[fileIndex][0].flags & SAVE_FLAG_FILE_EXISTS) != 0;
}

#line 420 "n64decomp/src/game/save_file.c"
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

#line 441 "n64decomp/src/game/save_file.c"
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

#line 455 "n64decomp/src/game/save_file.c"
s32 save_file_get_total_star_count(s32 fileIndex, s32 minCourse, s32 maxCourse) {
    s32 count = 0;

    // Get standard course star count.
    for (; minCourse <= maxCourse; minCourse++) {
        count += save_file_get_course_star_count(fileIndex, minCourse);
    }

    // Add castle secret star count.
    return save_file_get_course_star_count(fileIndex, COURSE_NUM_TO_INDEX(COURSE_NONE)) + count;
}

#line 467 "n64decomp/src/game/save_file.c"
void save_file_set_flags(u32 flags) {
    gSaveBuffer.files[gCurrSaveFileNum - 1][0].flags |= (flags | SAVE_FLAG_FILE_EXISTS);
    gSaveFileModified = TRUE;
}

#line 472 "n64decomp/src/game/save_file.c"
void save_file_clear_flags(u32 flags) {
    gSaveBuffer.files[gCurrSaveFileNum - 1][0].flags &= ~flags;
    gSaveBuffer.files[gCurrSaveFileNum - 1][0].flags |= SAVE_FLAG_FILE_EXISTS;
    gSaveFileModified = TRUE;
}

#line 478 "n64decomp/src/game/save_file.c"
u32 save_file_get_flags(void) {
    if (gCurrCreditsEntry != NULL || gCurrDemoInput != NULL) {
        return 0;
    }
    return gSaveBuffer.files[gCurrSaveFileNum - 1][0].flags;
}

#line 489 "n64decomp/src/game/save_file.c"
u32 save_file_get_star_flags(s32 fileIndex, s32 courseIndex) {
    u32 starFlags;

    if (courseIndex == COURSE_NUM_TO_INDEX(COURSE_NONE)) {
        starFlags = SAVE_FLAG_TO_STAR_FLAG(gSaveBuffer.files[fileIndex][0].flags);
    } else {
        starFlags = gSaveBuffer.files[fileIndex][0].courseStars[courseIndex] & 0x7F;
    }

    return starFlags;
}

#line 505 "n64decomp/src/game/save_file.c"
void save_file_set_star_flags(s32 fileIndex, s32 courseIndex, u32 starFlags) {
    if (courseIndex == COURSE_NUM_TO_INDEX(COURSE_NONE)) {
        gSaveBuffer.files[fileIndex][0].flags |= STAR_FLAG_TO_SAVE_FLAG(starFlags);
    } else {
        gSaveBuffer.files[fileIndex][0].courseStars[courseIndex] |= starFlags;
    }

    gSaveBuffer.files[fileIndex][0].flags |= SAVE_FLAG_FILE_EXISTS;
    gSaveFileModified = TRUE;
}

#line 516 "n64decomp/src/game/save_file.c"
s32 save_file_get_course_coin_score(s32 fileIndex, s32 courseIndex) {
    return gSaveBuffer.files[fileIndex][0].courseCoinScores[courseIndex];
}

#line 523 "n64decomp/src/game/save_file.c"
s32 save_file_is_cannon_unlocked(void) {
    return (gSaveBuffer.files[gCurrSaveFileNum - 1][0].courseStars[gCurrCourseNum] & (1 << 7)) != 0;
}

#line 530 "n64decomp/src/game/save_file.c"
void save_file_set_cannon_unlocked(void) {
    gSaveBuffer.files[gCurrSaveFileNum - 1][0].courseStars[gCurrCourseNum] |= (1 << 7);
    gSaveBuffer.files[gCurrSaveFileNum - 1][0].flags |= SAVE_FLAG_FILE_EXISTS;
    gSaveFileModified = TRUE;
}

#line 536 "n64decomp/src/game/save_file.c"
void save_file_set_cap_pos(s16 x, s16 y, s16 z) {
    struct SaveFile *saveFile = &gSaveBuffer.files[gCurrSaveFileNum - 1][0];

    saveFile->capLevel = gCurrLevelNum;
    saveFile->capArea = gCurrAreaIndex;
    vec3s_set(saveFile->capPos, x, y, z);
    save_file_set_flags(SAVE_FLAG_CAP_ON_GROUND);
}

#line 545 "n64decomp/src/game/save_file.c"
s32 save_file_get_cap_pos(Vec3s capPos) {
    struct SaveFile *saveFile = &gSaveBuffer.files[gCurrSaveFileNum - 1][0];
    s32 flags = save_file_get_flags();

    if (saveFile->capLevel == gCurrLevelNum && saveFile->capArea == gCurrAreaIndex
        && (flags & SAVE_FLAG_CAP_ON_GROUND)) {
        vec3s_copy(capPos, saveFile->capPos);
        return TRUE;
    }
    return FALSE;
}

#line 557 "n64decomp/src/game/save_file.c"
void save_file_set_sound_mode(u16 mode) {
    set_sound_mode(mode);
    gSaveBuffer.menuData[0].soundMode = mode;

    gMainMenuDataModified = TRUE;
    save_main_menu_data();
}

#line 565 "n64decomp/src/game/save_file.c"
u16 save_file_get_sound_mode(void) {
    return gSaveBuffer.menuData[0].soundMode;
}

#line 569 "n64decomp/src/game/save_file.c"
void save_file_move_cap_to_default_location(void) {
    if (save_file_get_flags() & SAVE_FLAG_CAP_ON_GROUND) {
        switch (gSaveBuffer.files[gCurrSaveFileNum - 1][0].capLevel) {
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

#line 598 "n64decomp/src/game/save_file.c"
void disable_warp_checkpoint(void) {
    // check_warp_checkpoint() checks to see if gWarpCheckpoint.courseNum != COURSE_NONE
    gWarpCheckpoint.courseNum = COURSE_NONE;
}

#line 607 "n64decomp/src/game/save_file.c"
void check_if_should_set_warp_checkpoint(struct WarpNode *warpNode) {
    if (warpNode->destLevel & 0x80) {
        // Overwrite the warp checkpoint variables.
        gWarpCheckpoint.actNum = gCurrActNum;
        gWarpCheckpoint.courseNum = gCurrCourseNum;
        gWarpCheckpoint.levelID = warpNode->destLevel & 0x7F;
        gWarpCheckpoint.areaNum = warpNode->destArea;
        gWarpCheckpoint.warpNode = warpNode->destNode;
    }
}

#line 623 "n64decomp/src/game/save_file.c"
s32 check_warp_checkpoint(struct WarpNode *warpNode) {
    s16 warpCheckpointActive = FALSE;
    s16 currCourseNum = gLevelToCourseNumTable[(warpNode->destLevel & 0x7F) - 1];

    // gSavedCourseNum is only used in this function.
    if (gWarpCheckpoint.courseNum != COURSE_NONE && gSavedCourseNum == currCourseNum
        && gWarpCheckpoint.actNum == gCurrActNum) {
        warpNode->destLevel = gWarpCheckpoint.levelID;
        warpNode->destArea = gWarpCheckpoint.areaNum;
        warpNode->destNode = gWarpCheckpoint.warpNode;
        warpCheckpointActive = TRUE;
    } else {
        // Disable the warp checkpoint just in case the other 2 conditions failed?
        gWarpCheckpoint.courseNum = COURSE_NONE;
    }

    return warpCheckpointActive;
}
