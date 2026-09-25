#include <ultra64.h>

#include "sm64.h"
#include "seq_ids.h"
#include "dialog_ids.h"
#include "audio/external.h"
#include "level_update.h"
#include "game_init.h"
#include "level_update.h"
#include "main.h"
#include "engine/math_util.h"
#include "engine/graph_node.h"
#include "area.h"
#include "save_file.h"
#include "sound_init.h"
#include "mario.h"
#include "camera.h"
#include "object_list_processor.h"
#include "ingame_menu.h"
#include "obj_behaviors.h"
#include "save_file.h"
#include "debug_course.h"
#ifdef VERSION_EU
#include "memory.h"
#include "eu_translation.h"
#include "segment_symbols.h"
#endif
#include "level_table.h"
#include "course_table.h"
#include "rumble_init.h"

#define PLAY_MODE_NORMAL 0
#define PLAY_MODE_PAUSED 2
#define PLAY_MODE_CHANGE_AREA 3
#define PLAY_MODE_CHANGE_LEVEL 4
#define PLAY_MODE_FRAME_ADVANCE 5

#define WARP_TYPE_NOT_WARPING 0
#define WARP_TYPE_CHANGE_LEVEL 1
#define WARP_TYPE_CHANGE_AREA 2
#define WARP_TYPE_SAME_AREA 3

// TODO: Make these ifdefs better
const char *credits01[] = { "1GAME DIRECTOR", "SHIGERU MIYAMOTO" };
const char *credits02[] = { "2ASSISTANT DIRECTORS", "YOSHIAKI KOIZUMI", "TAKASHI TEZUKA" };
const char *credits03[] = { "2SYSTEM PROGRAMMERS", "YASUNARI NISHIDA", "YOSHINORI TANIMOTO" };
const char *credits04[] = { "3PROGRAMMERS", "HAJIME YAJIMA", "DAIKI IWAMOTO", "TOSHIO IWAWAKI" };

#if defined(VERSION_JP) || defined(VERSION_SH) || defined(VERSION_CN)

const char *credits05[] = { "1CAMERA PROGRAMMER", "TAKUMI KAWAGOE" };
const char *credits06[] = { "1MARIO FACE PROGRAMMER", "GILES GODDARD" };
const char *credits07[] = { "2COURSE DIRECTORS", "YOICHI YAMADA", "YASUHISA YAMAMURA" };
const char *credits08[] = { "2COURSE DESIGNERS", "KENTA USUI", "NAOKI MORI" };
const char *credits09[] = { "3COURSE DESIGNERS", "YOSHIKI HARUHANA", "MAKOTO MIYANAGA", "KATSUHIKO KANNO" };
const char *credits10[] = { "1SOUND COMPOSER", "KOJI KONDO" };

#ifdef VERSION_JP
const char *credits11[] = { "1SOUND EFFECTS", "YOJI INAGAKI" };
const char *credits12[] = { "1SOUND PROGRAMMER", "HIDEAKI SHIMIZU" };
const char *credits13[] = { "23D ANIMATORS", "YOSHIAKI KOIZUMI", "SATORU TAKIZAWA" };
const char *credits14[] = { "1CG DESIGNER", "MASANAO ARIMOTO" };
const char *credits15[] = { "3TECHNICAL SUPPORT", "TAKAO SAWANO", "HIROHITO YOSHIMOTO", "HIROTO YADA" };
const char *credits16[] = { "1TECHNICAL SUPPORT", "SGI. 64PROJECT STAFF" };
const char *credits17[] = { "2PROGRESS MANAGEMENT", "KIMIYOSHI FUKUI", "KEIZO KATO" };
#else // VERSION_SH || VERSION_CN
// Shindou and iQue combine sound effects and sound programmer in order to make room for Mario voice and Peach voice
const char *credits11[] = { "4SOUND EFFECTS", "SOUND PROGRAMMER", "YOJI INAGAKI", "HIDEAKI SHIMIZU" };
const char *credits12[] = { "23D ANIMATORS", "YOSHIAKI KOIZUMI", "SATORU TAKIZAWA" };
const char *credits13[] = { "1CG DESIGNER", "MASANAO ARIMOTO" };
const char *credits14[] = { "3TECHNICAL SUPPORT", "TAKAO SAWANO", "HIROHITO YOSHIMOTO", "HIROTO YADA" };
const char *credits15[] = { "1TECHNICAL SUPPORT", "SGI. 64PROJECT STAFF" };
const char *credits16[] = { "2PROGRESS MANAGEMENT", "KIMIYOSHI FUKUI", "KEIZO KATO" };
#endif

#else // VERSION_US || VERSION_EU

// US and EU combine camera programmer and Mario face programmer...
const char *credits05[] = { "4CAMERA PROGRAMMER", "MARIO FACE PROGRAMMER", "TAKUMI KAWAGOE", "GILES GODDARD" };
const char *credits06[] = { "2COURSE DIRECTORS", "YOICHI YAMADA", "YASUHISA YAMAMURA" };
const char *credits07[] = { "2COURSE DESIGNERS", "KENTA USUI", "NAOKI MORI" };
const char *credits08[] = { "3COURSE DESIGNERS", "YOSHIKI HARUHANA", "MAKOTO MIYANAGA", "KATSUHIKO KANNO" };

#ifdef VERSION_US
const char *credits09[] = { "1SOUND COMPOSER", "KOJI KONDO" };
// ...as well as sound effects and sound programmer in order to make room for screen text writer, Mario voice, and Peach voice
const char *credits10[] = { "4SOUND EFFECTS", "SOUND PROGRAMMER", "YOJI INAGAKI", "HIDEAKI SHIMIZU" };
const char *credits11[] = { "23-D ANIMATORS", "YOSHIAKI KOIZUMI", "SATORU TAKIZAWA" };
const char *credits12[] = { "1ADDITIONAL GRAPHICS", "MASANAO ARIMOTO" };
const char *credits13[] = { "3TECHNICAL SUPPORT", "TAKAO SAWANO", "HIROHITO YOSHIMOTO", "HIROTO YADA" };
const char *credits14[] = { "1TECHNICAL SUPPORT", "SGI N64 PROJECT STAFF" };
const char *credits15[] = { "2PROGRESS MANAGEMENT", "KIMIYOSHI FUKUI", "KEIZO KATO" };
const char *credits16[] = { "5SCREEN TEXT WRITER", "TRANSLATION", "LESLIE SWAN", "MINA AKINO", "HIRO YAMADA" };
#else // VERSION_EU
// ...as well as sound composer, sound effects, and sound programmer, and...
const char *credits09[] = { "7SOUND COMPOSER", "SOUND EFFECTS", "SOUND PROGRAMMER", "KOJI KONDO", "YOJI INAGAKI", "HIDEAKI SHIMIZU" };
// ...3D animators and additional graphics in order to make room for screen text writer(s), Mario voice, and Peach voice
const char *credits10[] = { "63-D ANIMATORS", "ADDITIONAL GRAPHICS", "YOSHIAKI KOIZUMI", "SATORU TAKIZAWA", "MASANAO ARIMOTO" };
const char *credits11[] = { "3TECHNICAL SUPPORT", "TAKAO SAWANO", "HIROHITO YOSHIMOTO", "HIROTO YADA" };
const char *credits12[] = { "1TECHNICAL SUPPORT", "SGI N64 PROJECT STAFF" };
const char *credits13[] = { "2PROGRESS MANAGEMENT", "KIMIYOSHI FUKUI", "KEIZO KATO" };
const char *credits14[] = { "5SCREEN TEXT WRITER", "ENGLISH TRANSLATION", "LESLIE SWAN", "MINA AKINO", "HIRO YAMADA" };
const char *credits15[] = { "4SCREEN TEXT WRITER", "FRENCH TRANSLATION", "JULIEN BARDAKOFF", "KENJI HARAGUCHI" };
const char *credits16[] = { "4SCREEN TEXT WRITER", "GERMAN TRANSLATION", "THOMAS GOERG", "THOMAS SPINDLER" };
#endif

#endif

#ifndef VERSION_JP
const char *credits17[] = { "4MARIO VOICE", "PEACH VOICE", "CHARLES MARTINET", "LESLIE SWAN" };
#endif

#if defined(VERSION_JP) || defined(VERSION_SH) || defined(VERSION_CN)
// iQue uses this despite Jyoho Kaihatubu being Japanese
const char *credits18[] = { "3SPECIAL THANKS TO", "JYOHO KAIHATUBU", "ALL NINTENDO", "MARIO CLUB STAFF" };
#elif defined(VERSION_US)
const char *credits18[] = { "3SPECIAL THANKS TO", "EAD STAFF", "ALL NINTENDO PERSONNEL", "MARIO CLUB STAFF" };
#else // VERSION_EU
const char *credits18[] = { "3SPECIAL THANKS TO", "EAD STAFF", "ALL NINTENDO PERSONNEL", "SUPER MARIO CLUB STAFF" };
#endif

#ifdef VERSION_CN
// iQue combines producer and executive producer in order to make room for China production
const char *credits19[] = { "4PRODUCER", "EXECUTIVE PRODUCER", "SHIGERU MIYAMOTO", "HIROSHI YAMAUCHI" };
const char *credits20[] = { "1CHINA PRODUCTION", "IQUE ENGINEERING" };
#else
const char *credits19[] = { "1PRODUCER", "SHIGERU MIYAMOTO" };
const char *credits20[] = { "1EXECUTIVE PRODUCER", "HIROSHI YAMAUCHI" };
#endif

struct CreditsEntry sCreditsSequence[] = {
    { LEVEL_CASTLE_GROUNDS, 1, 1, -128, { 0, 8000, 0 }, NULL },
    { LEVEL_BOB, 1, 1, 117, { 713, 3918, -3889 }, credits01 },
    { LEVEL_WF, 1, 50, 46, { 347, 5376, 326 }, credits02 },
    { LEVEL_JRB, 1, 18, 22, { 3800, -4840, 2727 }, credits03 },
    { LEVEL_CCM, 2, 34, 25, { -5464, 6656, -6575 }, credits04 },
    { LEVEL_BBH, 1, 1, 60, { 257, 1922, 2580 }, credits05 },
    { LEVEL_HMC, 1, -15, 123, { -6469, 1616, -6054 }, credits06 },
    { LEVEL_THI, 3, 17, -32, { 508, 1024, 1942 }, credits07 },
    { LEVEL_LLL, 2, 33, 124, { -73, 82, -1467 }, credits08 },
    { LEVEL_SSL, 1, 65, 98, { -5906, 1024, -2576 }, credits09 },
    { LEVEL_DDD, 1, 50, 47, { -4884, -4607, -272 }, credits10 },
    { LEVEL_SL, 1, 17, -34, { 1925, 3328, 563 }, credits11 },
    { LEVEL_WDW, 1, 33, 105, { -537, 1850, 1818 }, credits12 },
    { LEVEL_TTM, 1, 2, -33, { 2613, 313, 1074 }, credits13 },
    { LEVEL_THI, 1, 51, 54, { -2609, 512, 856 }, credits14 },
    { LEVEL_TTC, 1, 17, -72, { -1304, -71, -967 }, credits15 },
    { LEVEL_RR, 1, 33, 64, { 1565, 1024, -148 }, credits16 },
    { LEVEL_SA, 1, 1, 24, { -1050, -1330, -1559 }, credits17 },
    { LEVEL_COTMC, 1, 49, -16, { -254, 415, -6045 }, credits18 },
    { LEVEL_DDD, 2, -111, -64, { 3948, 1185, -104 }, credits19 },
    { LEVEL_CCM, 1, 33, 31, { 3169, -4607, 5240 }, credits20 },
    { LEVEL_CASTLE_GROUNDS, 1, 1, -128, { 0, 906, -1200 }, NULL },
    { LEVEL_NONE, 0, 1, 0, { 0, 0, 0 }, NULL },
};

struct MarioState gMarioStates[1];
struct HudDisplay gHudDisplay;

FORCE_BSS s16 sCurrPlayMode;
FORCE_BSS u16 D_80339ECA;
FORCE_BSS s16 sTransitionTimer;
FORCE_BSS void (*sTransitionUpdate)(s16 *);
FORCE_BSS struct WarpDest sWarpDest;
FORCE_BSS s16 D_80339EE0;
FORCE_BSS s16 sDelayedWarpOp;
FORCE_BSS s16 sDelayedWarpTimer;
FORCE_BSS s16 sSourceWarpNodeId;
FORCE_BSS s32 sDelayedWarpArg;
FORCE_BSS s16 sUnusedLevelUpdateBss;
FORCE_BSS s8 sTimerRunning;
s8 gNeverEnteredCastle;

struct MarioState *gMarioState = &gMarioStates[0];
u8 unused1[2] = { 0 };
s8 sWarpCheckpointActive = FALSE;
u8 unused2[4];

u16 level_control_timer(s32 timerOp) {
    switch (timerOp) {
        case TIMER_CONTROL_SHOW:
            WORLD(gHudDisplay).flags |= HUD_DISPLAY_FLAG_TIMER;
            WORLD(sTimerRunning) = FALSE;
            WORLD(gHudDisplay).timer = 0;
            break;

        case TIMER_CONTROL_START:
            WORLD(sTimerRunning) = TRUE;
            break;

        case TIMER_CONTROL_STOP:
            WORLD(sTimerRunning) = FALSE;
            break;

        case TIMER_CONTROL_HIDE:
            WORLD(gHudDisplay).flags &= ~HUD_DISPLAY_FLAG_TIMER;
            WORLD(sTimerRunning) = FALSE;
            WORLD(gHudDisplay).timer = 0;
            break;
    }

    return WORLD(gHudDisplay).timer;
}

u32 pressed_pause(void) {
    u32 dialogActive = get_dialog_id() >= 0;
    u32 intangible = (WORLD(gMarioState)->action & ACT_FLAG_INTANGIBLE) != 0;

    if (!intangible && !dialogActive && !WORLD(gWarpTransition).isActive && WORLD(sDelayedWarpOp) == WARP_OP_NONE
        && (WORLD(gPlayer1Controller)->buttonPressed & START_BUTTON)) {
        return TRUE;
    }

    return FALSE;
}

void set_play_mode(s16 playMode) {
    WORLD(sCurrPlayMode) = playMode;
    WORLD(D_80339ECA) = 0;
}

void warp_special(s32 arg) {
    WORLD(sCurrPlayMode) = PLAY_MODE_CHANGE_LEVEL;
    WORLD(D_80339ECA) = 0;
    WORLD(D_80339EE0) = arg;
}

void fade_into_special_warp(u32 arg, u32 color) {
    if (color != 0) {
        color = 0xFF;
    }

    fadeout_music(190);
    play_transition(WARP_TRANSITION_FADE_INTO_COLOR, 0x10, color, color, color);
    level_set_transition(30, NULL);

    warp_special(arg);
}

void stub_level_update_1(void) {
}

void load_level_init_text(u32 arg) {
    s32 gotAchievement;
    s32 dialogID = WORLD(gCurrentArea)->dialog[arg];

    switch (dialogID) {
        case DIALOG_129:
            gotAchievement = save_file_get_flags() & SAVE_FLAG_HAVE_VANISH_CAP;
            break;

        case DIALOG_130:
            gotAchievement = save_file_get_flags() & SAVE_FLAG_HAVE_METAL_CAP;
            break;

        case DIALOG_131:
            gotAchievement = save_file_get_flags() & SAVE_FLAG_HAVE_WING_CAP;
            break;

        case (u8)DIALOG_NONE: // 255, cast value to u8 to match (-1)
            gotAchievement = TRUE;
            break;

        default:
            gotAchievement =
                save_file_get_star_flags(WORLD(gCurrSaveFileNum) - 1, COURSE_NUM_TO_INDEX(WORLD(gCurrCourseNum)));
            break;
    }

    if (!gotAchievement) {
        level_set_transition(-1, NULL);
        create_dialog_box(dialogID);
    }
}

void init_door_warp(struct SpawnInfo *spawnInfo, u32 arg1) {
    if (arg1 & 0x00000002) {
        spawnInfo->startAngle[1] += 0x8000;
    }

    spawnInfo->startPos[0] += 300.0f * sins(spawnInfo->startAngle[1]);
    spawnInfo->startPos[2] += 300.0f * coss(spawnInfo->startAngle[1]);
}

void set_mario_initial_cap_powerup(struct MarioState *m) {
    s32 capCourseIndex = WORLD(gCurrCourseNum) - COURSE_CAP_COURSES;

    switch (capCourseIndex) {
        case COURSE_COTMC - COURSE_CAP_COURSES:
            m->flags |= MARIO_METAL_CAP | MARIO_CAP_ON_HEAD;
            m->capTimer = 600;
            break;

        case COURSE_TOTWC - COURSE_CAP_COURSES:
            m->flags |= MARIO_WING_CAP | MARIO_CAP_ON_HEAD;
            m->capTimer = 1200;
            break;

        case COURSE_VCUTM - COURSE_CAP_COURSES:
            m->flags |= MARIO_VANISH_CAP | MARIO_CAP_ON_HEAD;
            m->capTimer = 600;
            break;
    }
}

void set_mario_initial_action(struct MarioState *m, u32 spawnType, u32 actionArg) {
    switch (spawnType) {
        case MARIO_SPAWN_DOOR_WARP:
            set_mario_action(m, ACT_WARP_DOOR_SPAWN, actionArg);
            break;
        case MARIO_SPAWN_UNKNOWN_02:
            set_mario_action(m, ACT_IDLE, 0);
            break;
        case MARIO_SPAWN_UNKNOWN_03:
            set_mario_action(m, ACT_EMERGE_FROM_PIPE, 0);
            break;
        case MARIO_SPAWN_TELEPORT:
            set_mario_action(m, ACT_TELEPORT_FADE_IN, 0);
            break;
        case MARIO_SPAWN_INSTANT_ACTIVE:
            set_mario_action(m, ACT_IDLE, 0);
            break;
        case MARIO_SPAWN_AIRBORNE:
            set_mario_action(m, ACT_SPAWN_NO_SPIN_AIRBORNE, 0);
            break;
        case MARIO_SPAWN_HARD_AIR_KNOCKBACK:
            set_mario_action(m, ACT_HARD_BACKWARD_AIR_KB, 0);
            break;
        case MARIO_SPAWN_SPIN_AIRBORNE_CIRCLE:
            set_mario_action(m, ACT_SPAWN_SPIN_AIRBORNE, 0);
            break;
        case MARIO_SPAWN_DEATH:
            set_mario_action(m, ACT_FALLING_DEATH_EXIT, 0);
            break;
        case MARIO_SPAWN_SPIN_AIRBORNE:
            set_mario_action(m, ACT_SPAWN_SPIN_AIRBORNE, 0);
            break;
        case MARIO_SPAWN_FLYING:
            set_mario_action(m, ACT_FLYING, 2);
            break;
        case MARIO_SPAWN_SWIMMING:
            set_mario_action(m, ACT_WATER_IDLE, 1);
            break;
        case MARIO_SPAWN_PAINTING_STAR_COLLECT:
            set_mario_action(m, ACT_EXIT_AIRBORNE, 0);
            break;
        case MARIO_SPAWN_PAINTING_DEATH:
            set_mario_action(m, ACT_DEATH_EXIT, 0);
            break;
        case MARIO_SPAWN_AIRBORNE_STAR_COLLECT:
            set_mario_action(m, ACT_FALLING_EXIT_AIRBORNE, 0);
            break;
        case MARIO_SPAWN_AIRBORNE_DEATH:
            set_mario_action(m, ACT_UNUSED_DEATH_EXIT, 0);
            break;
        case MARIO_SPAWN_LAUNCH_STAR_COLLECT:
            set_mario_action(m, ACT_SPECIAL_EXIT_AIRBORNE, 0);
            break;
        case MARIO_SPAWN_LAUNCH_DEATH:
            set_mario_action(m, ACT_SPECIAL_DEATH_EXIT, 0);
            break;
    }

    set_mario_initial_cap_powerup(m);
}

void init_mario_after_warp(void) {
    N64_STACK_FRAME(init_mario_after_warp);
    struct ObjectWarpNode *spawnNode = area_get_warp_node(WORLD(sWarpDest).nodeId);
    u32 marioSpawnType = get_mario_spawn_type(spawnNode->object);

    if (WORLD(gMarioState)->action != ACT_UNINITIALIZED) {
        WORLD(gPlayerSpawnInfos)[0].startPos[0] = (s16) spawnNode->object->oPosX;
        WORLD(gPlayerSpawnInfos)[0].startPos[1] = (s16) spawnNode->object->oPosY;
        WORLD(gPlayerSpawnInfos)[0].startPos[2] = (s16) spawnNode->object->oPosZ;

        WORLD(gPlayerSpawnInfos)[0].startAngle[0] = 0;
        WORLD(gPlayerSpawnInfos)[0].startAngle[1] = spawnNode->object->oMoveAngleYaw;
        WORLD(gPlayerSpawnInfos)[0].startAngle[2] = 0;

        if (marioSpawnType == MARIO_SPAWN_DOOR_WARP) {
            init_door_warp(&WORLD(gPlayerSpawnInfos)[0], WORLD(sWarpDest).arg);
        }

        if (WORLD(sWarpDest).type == WARP_TYPE_CHANGE_LEVEL || WORLD(sWarpDest).type == WARP_TYPE_CHANGE_AREA) {
            WORLD(gPlayerSpawnInfos)[0].areaIndex = WORLD(sWarpDest).areaIdx;
            load_mario_area();
        }

        init_mario();
        set_mario_initial_action(WORLD(gMarioState), marioSpawnType, WORLD(sWarpDest).arg);

        WORLD(gMarioState)->interactObj = spawnNode->object;
        WORLD(gMarioState)->usedObj = spawnNode->object;
    }

    reset_camera(WORLD(gCurrentArea)->camera);
    WORLD(sWarpDest).type = WARP_TYPE_NOT_WARPING;
    WORLD(sDelayedWarpOp) = WARP_OP_NONE;

    switch (marioSpawnType) {
        case MARIO_SPAWN_UNKNOWN_03:
            play_transition(WARP_TRANSITION_FADE_FROM_STAR, 0x10, 0x00, 0x00, 0x00);
            break;
        case MARIO_SPAWN_DOOR_WARP:
            play_transition(WARP_TRANSITION_FADE_FROM_CIRCLE, 0x10, 0x00, 0x00, 0x00);
            break;
        case MARIO_SPAWN_TELEPORT:
            play_transition(WARP_TRANSITION_FADE_FROM_COLOR, 0x14, 0xFF, 0xFF, 0xFF);
            break;
        case MARIO_SPAWN_SPIN_AIRBORNE:
            play_transition(WARP_TRANSITION_FADE_FROM_COLOR, 0x1A, 0xFF, 0xFF, 0xFF);
            break;
        case MARIO_SPAWN_SPIN_AIRBORNE_CIRCLE:
            play_transition(WARP_TRANSITION_FADE_FROM_CIRCLE, 0x10, 0x00, 0x00, 0x00);
            break;
        case MARIO_SPAWN_UNKNOWN_27:
            play_transition(WARP_TRANSITION_FADE_FROM_COLOR, 0x10, 0x00, 0x00, 0x00);
            break;
        default:
            play_transition(WARP_TRANSITION_FADE_FROM_STAR, 0x10, 0x00, 0x00, 0x00);
            break;
    }

    if (WORLD(gCurrDemoInput) == NULL) {
        set_background_music(WORLD(gCurrentArea)->musicParam, WORLD(gCurrentArea)->musicParam2, 0);

        if (WORLD(gMarioState)->flags & MARIO_METAL_CAP) {
            play_cap_music(SEQUENCE_ARGS(4, SEQ_EVENT_METAL_CAP));
        }

        if (WORLD(gMarioState)->flags & (MARIO_VANISH_CAP | MARIO_WING_CAP)) {
            play_cap_music(SEQUENCE_ARGS(4, SEQ_EVENT_POWERUP));
        }

#if BUGFIX_KOOPA_RACE_MUSIC
        if (WORLD(gCurrLevelNum) == LEVEL_BOB
            && get_current_background_music() != SEQUENCE_ARGS(4, SEQ_LEVEL_SLIDE) && WORLD(sTimerRunning)) {
            play_music(SEQ_PLAYER_LEVEL, SEQUENCE_ARGS(4, SEQ_LEVEL_SLIDE), 0);
        }
#endif

        if (WORLD(sWarpDest).levelNum == LEVEL_CASTLE && WORLD(sWarpDest).areaIdx == 1
#ifndef VERSION_JP
            && (WORLD(sWarpDest).nodeId == 31 || WORLD(sWarpDest).nodeId == 32)
#else
            && WORLD(sWarpDest).nodeId == 31
#endif
        ) {
            play_sound(SOUND_MENU_MARIO_CASTLE_WARP, WORLD(gGlobalSoundSource));
        }

#ifndef VERSION_JP
        if (WORLD(sWarpDest).levelNum == LEVEL_CASTLE_GROUNDS && WORLD(sWarpDest).areaIdx == 1
            && (WORLD(sWarpDest).nodeId == 7 || WORLD(sWarpDest).nodeId == 10 || WORLD(sWarpDest).nodeId == 20
                || WORLD(sWarpDest).nodeId == 30)) {
            play_sound(SOUND_MENU_MARIO_CASTLE_WARP, WORLD(gGlobalSoundSource));
        }
#endif
    }
}

// used for warps inside one level
void warp_area(void) {
    N64_STACK_FRAME(warp_area);
    if (WORLD(sWarpDest).type != WARP_TYPE_NOT_WARPING) {
        if (WORLD(sWarpDest).type == WARP_TYPE_CHANGE_AREA) {
            level_control_timer(TIMER_CONTROL_HIDE);
            unload_mario_area();
            load_area(WORLD(sWarpDest).areaIdx);
        }

        init_mario_after_warp();
    }
}

// used for warps between levels
void warp_level(void) {
    N64_STACK_FRAME(warp_level);
    WORLD(gCurrLevelNum) = WORLD(sWarpDest).levelNum;

    level_control_timer(TIMER_CONTROL_HIDE);

    load_area(WORLD(sWarpDest).areaIdx);
    init_mario_after_warp();
}

void warp_credits(void) {
    N64_STACK_FRAME(warp_credits);
    s32 marioAction;

    switch (WORLD(sWarpDest).nodeId) {
        case WARP_NODE_CREDITS_START:
            marioAction = ACT_END_PEACH_CUTSCENE;
            break;

        case WARP_NODE_CREDITS_NEXT:
            marioAction = ACT_CREDITS_CUTSCENE;
            break;

        case WARP_NODE_CREDITS_END:
            marioAction = ACT_END_WAVING_CUTSCENE;
            break;
    }

    WORLD(gCurrLevelNum) = WORLD(sWarpDest).levelNum;

    load_area(WORLD(sWarpDest).areaIdx);

    vec3s_set(WORLD(gPlayerSpawnInfos)[0].startPos, WORLD(gCurrCreditsEntry)->marioPos[0],
              WORLD(gCurrCreditsEntry)->marioPos[1], WORLD(gCurrCreditsEntry)->marioPos[2]);

    vec3s_set(WORLD(gPlayerSpawnInfos)[0].startAngle, 0, WORLD(gCurrCreditsEntry)->marioAngle << 8, 0);

    WORLD(gPlayerSpawnInfos)[0].areaIndex = WORLD(sWarpDest).areaIdx;

    load_mario_area();
    init_mario();

    set_mario_action(WORLD(gMarioState), marioAction, 0);

    reset_camera(WORLD(gCurrentArea)->camera);

    WORLD(sWarpDest).type = WARP_TYPE_NOT_WARPING;
    WORLD(sDelayedWarpOp) = WARP_OP_NONE;

    play_transition(WARP_TRANSITION_FADE_FROM_COLOR, 0x14, 0x00, 0x00, 0x00);

    if (WORLD(gCurrCreditsEntry) == NULL || WORLD(gCurrCreditsEntry) == WORLD(sCreditsSequence)) {
        set_background_music(WORLD(gCurrentArea)->musicParam, WORLD(gCurrentArea)->musicParam2, 0);
    }
}

void check_instant_warp(void) {
    N64_STACK_FRAME(check_instant_warp);
    s16 cameraAngle;
    struct Surface *floor;

    if (WORLD(gCurrLevelNum) == LEVEL_CASTLE
        && save_file_get_total_star_count(WORLD(gCurrSaveFileNum) - 1, COURSE_MIN - 1, COURSE_MAX - 1) >= 70) {
        return;
    }

    if ((floor = WORLD(gMarioState)->floor) != NULL) {
        s32 index = floor->type - SURFACE_INSTANT_WARP_1B;
        if (index >= INSTANT_WARP_INDEX_START && index < INSTANT_WARP_INDEX_STOP
            && WORLD(gCurrentArea)->instantWarps != NULL) {
            struct InstantWarp *warp = &WORLD(gCurrentArea)->instantWarps[index];

            if (warp->id != 0) {
                WORLD(gMarioState)->pos[0] += warp->displacement[0];
                WORLD(gMarioState)->pos[1] += warp->displacement[1];
                WORLD(gMarioState)->pos[2] += warp->displacement[2];

                WORLD(gMarioState)->marioObj->oPosX = WORLD(gMarioState)->pos[0];
                WORLD(gMarioState)->marioObj->oPosY = WORLD(gMarioState)->pos[1];
                WORLD(gMarioState)->marioObj->oPosZ = WORLD(gMarioState)->pos[2];

                cameraAngle = WORLD(gMarioState)->area->camera->yaw;

                change_area(warp->area);
                WORLD(gMarioState)->area = WORLD(gCurrentArea);

                warp_camera(warp->displacement[0], warp->displacement[1], warp->displacement[2]);

                WORLD(gMarioState)->area->camera->yaw = cameraAngle;
            }
        }
    }
}

s16 music_changed_through_warp(s16 arg) {
    struct ObjectWarpNode *warpNode = area_get_warp_node(arg);
    s16 levelNum = warpNode->node.destLevel & 0x7F;

#if BUGFIX_KOOPA_RACE_MUSIC

    s16 destArea = warpNode->node.destArea;
    s16 val4 = TRUE;
    s16 sp2C;

    if (levelNum == LEVEL_BOB && levelNum == WORLD(gCurrLevelNum) && destArea == WORLD(gCurrAreaIndex)) {
        sp2C = get_current_background_music();
        if (sp2C == SEQUENCE_ARGS(4, SEQ_EVENT_POWERUP | SEQ_VARIATION)
            || sp2C == SEQUENCE_ARGS(4, SEQ_EVENT_POWERUP)) {
            val4 = FALSE;
        }
    } else {
        u16 val8 = WORLD(gAreas)[destArea].musicParam;
        u16 val6 = WORLD(gAreas)[destArea].musicParam2;

        val4 = levelNum == WORLD(gCurrLevelNum) && val8 == WORLD(gCurrentArea)->musicParam
               && val6 == WORLD(gCurrentArea)->musicParam2;

        if (get_current_background_music() != val6) {
            val4 = FALSE;
        }
    }
    return val4;

#else

    u16 val8 = WORLD(gAreas)[warpNode->node.destArea].musicParam;
    u16 val6 = WORLD(gAreas)[warpNode->node.destArea].musicParam2;

    s16 val4 = levelNum == WORLD(gCurrLevelNum) && val8 == WORLD(gCurrentArea)->musicParam
               && val6 == WORLD(gCurrentArea)->musicParam2;

    if (get_current_background_music() != val6) {
        val4 = FALSE;
    }
    return val4;

#endif
}

/**
 * Set the current warp type and destination level/area/node.
 */
void initiate_warp(s16 destLevel, s16 destArea, s16 destWarpNode, s32 arg3) {
    if (destWarpNode >= WARP_NODE_CREDITS_MIN) {
        WORLD(sWarpDest).type = WARP_TYPE_CHANGE_LEVEL;
    } else if (destLevel != WORLD(gCurrLevelNum)) {
        WORLD(sWarpDest).type = WARP_TYPE_CHANGE_LEVEL;
    } else if (destArea != WORLD(gCurrentArea)->index) {
        WORLD(sWarpDest).type = WARP_TYPE_CHANGE_AREA;
    } else {
        WORLD(sWarpDest).type = WARP_TYPE_SAME_AREA;
    }

    WORLD(sWarpDest).levelNum = destLevel;
    WORLD(sWarpDest).areaIdx = destArea;
    WORLD(sWarpDest).nodeId = destWarpNode;
    WORLD(sWarpDest).arg = arg3;
}

// From Surface 0xD3 to 0xFC
#define PAINTING_WARP_INDEX_START 0x00 // Value greater than or equal to Surface 0xD3
#define PAINTING_WARP_INDEX_FA 0x2A    // THI Huge Painting index left
#define PAINTING_WARP_INDEX_END 0x2D   // Value less than Surface 0xFD

/**
 * Check if Mario is above and close to a painting warp floor, and return the
 * corresponding warp node.
 */
struct WarpNode *get_painting_warp_node(void) {
    struct WarpNode *warpNode = NULL;
    s32 paintingIndex = WORLD(gMarioState)->floor->type - SURFACE_PAINTING_WARP_D3;

    if (paintingIndex >= PAINTING_WARP_INDEX_START && paintingIndex < PAINTING_WARP_INDEX_END) {
        if (paintingIndex < PAINTING_WARP_INDEX_FA
            || WORLD(gMarioState)->pos[1] - WORLD(gMarioState)->floorHeight < 80.0f) {
            warpNode = &WORLD(gCurrentArea)->paintingWarpNodes[paintingIndex];
        }
    }

    return warpNode;
}

/**
 * Check is Mario has entered a painting, and if so, initiate a warp.
 */
void initiate_painting_warp(void) {
    if (WORLD(gCurrentArea)->paintingWarpNodes != NULL && WORLD(gMarioState)->floor != NULL) {
        struct WarpNode warpNode;
        struct WarpNode *pWarpNode = get_painting_warp_node();

        if (pWarpNode != NULL) {
            if (WORLD(gMarioState)->action & ACT_FLAG_INTANGIBLE) {
                play_painting_eject_sound();
            } else if (pWarpNode->id != 0) {
                warpNode = *pWarpNode;

                if (!(warpNode.destLevel & 0x80)) {
                    WORLD(sWarpCheckpointActive) = check_warp_checkpoint(&warpNode);
                }

                initiate_warp(warpNode.destLevel & 0x7F, warpNode.destArea, warpNode.destNode, 0);
                check_if_should_set_warp_checkpoint(&warpNode);

                play_transition_after_delay(WARP_TRANSITION_FADE_INTO_COLOR, 30, 255, 255, 255, 45);
                level_set_transition(74, basic_update);

                set_mario_action(WORLD(gMarioState), ACT_DISAPPEARED, 0);

                WORLD(gMarioState)->marioObj->header.gfx.node.flags &= ~GRAPH_RENDER_ACTIVE;

                play_sound(SOUND_MENU_STAR_SOUND, WORLD(gGlobalSoundSource));
                fadeout_music(398);
#if ENABLE_RUMBLE
                queue_rumble_data(80, 70);
                func_sh_8024C89C(1);
#endif
            }
        }
    }
}

/**
 * If there is not already a delayed warp, schedule one. The source node is
 * based on the warp operation and sometimes Mario's used object.
 * Return the time left until the delayed warp is initiated.
 */
s16 level_trigger_warp(struct MarioState *m, s32 warpOp) {
    s32 val04 = TRUE;

    if (WORLD(sDelayedWarpOp) == WARP_OP_NONE) {
        m->invincTimer = -1;
        WORLD(sDelayedWarpArg) = 0;
        WORLD(sDelayedWarpOp) = warpOp;

        switch (warpOp) {
            case WARP_OP_DEMO_NEXT:
            case WARP_OP_DEMO_END: WORLD(sDelayedWarpTimer) = 20; // Must be one line to match on -O2
                WORLD(sSourceWarpNodeId) = WARP_NODE_SUCCESS;
                WORLD(gSavedCourseNum) = COURSE_NONE;
                val04 = FALSE;
                play_transition(WARP_TRANSITION_FADE_INTO_STAR, 0x14, 0x00, 0x00, 0x00);
                break;

            case WARP_OP_CREDITS_END:
                WORLD(sDelayedWarpTimer) = 60;
                WORLD(sSourceWarpNodeId) = WARP_NODE_SUCCESS;
                val04 = FALSE;
                WORLD(gSavedCourseNum) = COURSE_NONE;
                play_transition(WARP_TRANSITION_FADE_INTO_COLOR, 0x3C, 0x00, 0x00, 0x00);
                break;

            case WARP_OP_STAR_EXIT:
                WORLD(sDelayedWarpTimer) = 32;
                WORLD(sSourceWarpNodeId) = WARP_NODE_SUCCESS;
                WORLD(gSavedCourseNum) = COURSE_NONE;
                play_transition(WARP_TRANSITION_FADE_INTO_MARIO, 0x20, 0x00, 0x00, 0x00);
                break;

            case WARP_OP_DEATH:
                if (m->numLives == 0) {
                    WORLD(sDelayedWarpOp) = WARP_OP_GAME_OVER;
                }
                WORLD(sDelayedWarpTimer) = 48;
                WORLD(sSourceWarpNodeId) = WARP_NODE_DEATH;
                play_transition(WARP_TRANSITION_FADE_INTO_BOWSER, 0x30, 0x00, 0x00, 0x00);
                play_sound(SOUND_MENU_BOWSER_LAUGH, WORLD(gGlobalSoundSource));
                break;

            case WARP_OP_WARP_FLOOR:
                WORLD(sSourceWarpNodeId) = WARP_NODE_WARP_FLOOR;
                if (area_get_warp_node(WORLD(sSourceWarpNodeId)) == NULL) {
                    if (m->numLives == 0) {
                        WORLD(sDelayedWarpOp) = WARP_OP_GAME_OVER;
                    } else {
                        WORLD(sSourceWarpNodeId) = WARP_NODE_DEATH;
                    }
                }
                WORLD(sDelayedWarpTimer) = 20;
                play_transition(WARP_TRANSITION_FADE_INTO_CIRCLE, 0x14, 0x00, 0x00, 0x00);
                break;

            case WARP_OP_UNKNOWN_01: // enter TotWC
                WORLD(sDelayedWarpTimer) = 30;
                WORLD(sSourceWarpNodeId) = WARP_NODE_TOTWC;
                play_transition(WARP_TRANSITION_FADE_INTO_COLOR, 0x1E, 0xFF, 0xFF, 0xFF);
#ifndef VERSION_JP
                play_sound(SOUND_MENU_STAR_SOUND, WORLD(gGlobalSoundSource));
#endif
                break;

            case WARP_OP_UNKNOWN_02: // enter BBH
                WORLD(sDelayedWarpTimer) = 30;
                WORLD(sSourceWarpNodeId) = (m->usedObj->oBhvParams & 0x00FF0000) >> 16;
                play_transition(WARP_TRANSITION_FADE_INTO_COLOR, 0x1E, 0xFF, 0xFF, 0xFF);
                break;

            case WARP_OP_TELEPORT:
                WORLD(sDelayedWarpTimer) = 20;
                WORLD(sSourceWarpNodeId) = (m->usedObj->oBhvParams & 0x00FF0000) >> 16;
                val04 = !music_changed_through_warp(WORLD(sSourceWarpNodeId));
                play_transition(WARP_TRANSITION_FADE_INTO_COLOR, 0x14, 0xFF, 0xFF, 0xFF);
                break;

            case WARP_OP_WARP_DOOR:
                WORLD(sDelayedWarpTimer) = 20;
                WORLD(sDelayedWarpArg) = m->actionArg;
                WORLD(sSourceWarpNodeId) = (m->usedObj->oBhvParams & 0x00FF0000) >> 16;
                val04 = !music_changed_through_warp(WORLD(sSourceWarpNodeId));
                play_transition(WARP_TRANSITION_FADE_INTO_CIRCLE, 0x14, 0x00, 0x00, 0x00);
                break;

            case WARP_OP_WARP_OBJECT:
                WORLD(sDelayedWarpTimer) = 20;
                WORLD(sSourceWarpNodeId) = (m->usedObj->oBhvParams & 0x00FF0000) >> 16;
                val04 = !music_changed_through_warp(WORLD(sSourceWarpNodeId));
                play_transition(WARP_TRANSITION_FADE_INTO_STAR, 0x14, 0x00, 0x00, 0x00);
                break;

            case WARP_OP_CREDITS_START:
                WORLD(sDelayedWarpTimer) = 30;
                play_transition(WARP_TRANSITION_FADE_INTO_COLOR, 0x1E, 0x00, 0x00, 0x00);
                break;

            case WARP_OP_CREDITS_NEXT:
                if (WORLD(gCurrCreditsEntry) == &WORLD(sCreditsSequence)[0]) {
                    WORLD(sDelayedWarpTimer) = 60;
                    play_transition(WARP_TRANSITION_FADE_INTO_COLOR, 0x3C, 0x00, 0x00, 0x00);
                } else {
                    WORLD(sDelayedWarpTimer) = 20;
                    play_transition(WARP_TRANSITION_FADE_INTO_COLOR, 0x14, 0x00, 0x00, 0x00);
                }
                val04 = FALSE;
                break;
        }

        if (val04 && WORLD(gCurrDemoInput) == NULL) {
            fadeout_music((3 * WORLD(sDelayedWarpTimer) / 2) * 8 - 2);
        }
    }

    return WORLD(sDelayedWarpTimer);
}

/**
 * If a delayed warp is ready, initiate it.
 */
void initiate_delayed_warp(void) {
    struct ObjectWarpNode *warpNode;
    s32 destWarpNode;

    if (WORLD(sDelayedWarpOp) != WARP_OP_NONE && --WORLD(sDelayedWarpTimer) == 0) {
        reset_dialog_render_state();

        if (WORLD(gDebugLevelSelect) && (WORLD(sDelayedWarpOp) & WARP_OP_TRIGGERS_LEVEL_SELECT)) {
            warp_special(-9);
        } else if (WORLD(gCurrDemoInput) != NULL) {
            if (WORLD(sDelayedWarpOp) == WARP_OP_DEMO_END) {
                warp_special(-8);
            } else {
                warp_special(-2);
            }
        } else {
            switch (WORLD(sDelayedWarpOp)) {
                case WARP_OP_GAME_OVER:
                    save_file_reload();
                    warp_special(-3);
                    break;

                case WARP_OP_CREDITS_END:
                    warp_special(-1);
                    sound_banks_enable(SEQ_PLAYER_SFX,
                                       SOUND_BANKS_ALL & ~SOUND_BANKS_DISABLED_AFTER_CREDITS);
                    break;

                case WARP_OP_DEMO_NEXT:
                    warp_special(-2);
                    break;

                case WARP_OP_CREDITS_START:
                    WORLD(gCurrCreditsEntry) = &WORLD(sCreditsSequence)[0];
                    initiate_warp(WORLD(gCurrCreditsEntry)->levelNum, WORLD(gCurrCreditsEntry)->areaIndex,
                                  WARP_NODE_CREDITS_START, 0);
                    break;

                case WARP_OP_CREDITS_NEXT:
                    sound_banks_disable(SEQ_PLAYER_SFX, SOUND_BANKS_ALL);

                    WORLD(gCurrCreditsEntry)++;
                    WORLD(gCurrActNum) = WORLD(gCurrCreditsEntry)->unk02 & 0x07;
                    if ((WORLD(gCurrCreditsEntry) + 1)->levelNum == LEVEL_NONE) {
                        destWarpNode = WARP_NODE_CREDITS_END;
                    } else {
                        destWarpNode = WARP_NODE_CREDITS_NEXT;
                    }

                    initiate_warp(WORLD(gCurrCreditsEntry)->levelNum, WORLD(gCurrCreditsEntry)->areaIndex,
                                  destWarpNode, 0);
                    break;

                default:
                    warpNode = area_get_warp_node(WORLD(sSourceWarpNodeId));

                    initiate_warp(warpNode->node.destLevel & 0x7F, warpNode->node.destArea,
                                  warpNode->node.destNode, WORLD(sDelayedWarpArg));

                    check_if_should_set_warp_checkpoint(&warpNode->node);
                    if (WORLD(sWarpDest).type != WARP_TYPE_CHANGE_LEVEL) {
                        level_set_transition(2, NULL);
                    }
                    break;
            }
        }
    }
}

void update_hud_values(void) {
    if (WORLD(gCurrCreditsEntry) == NULL) {
        s16 numHealthWedges = WORLD(gMarioState)->health > 0 ? WORLD(gMarioState)->health >> 8 : 0;

        if (WORLD(gCurrCourseNum) >= COURSE_MIN) {
            WORLD(gHudDisplay).flags |= HUD_DISPLAY_FLAG_COIN_COUNT;
        } else {
            WORLD(gHudDisplay).flags &= ~HUD_DISPLAY_FLAG_COIN_COUNT;
        }

        if (WORLD(gHudDisplay).coins < WORLD(gMarioState)->numCoins) {
            if (WORLD(gGlobalTimer) & 1) {
                u32 coinSound;
                if (WORLD(gMarioState)->action & (ACT_FLAG_SWIMMING | ACT_FLAG_METAL_WATER)) {
                    coinSound = SOUND_GENERAL_COIN_WATER;
                } else {
                    coinSound = SOUND_GENERAL_COIN;
                }

                WORLD(gHudDisplay).coins++;
                play_sound(coinSound, WORLD(gMarioState)->marioObj->header.gfx.cameraToObject);
            }
        }

        if (WORLD(gMarioState)->numLives > 100) {
            WORLD(gMarioState)->numLives = 100;
        }

#if BUGFIX_MAX_LIVES
        if (WORLD(gMarioState)->numCoins > 999) {
            WORLD(gMarioState)->numCoins = 999;
        }

        if (WORLD(gHudDisplay).coins > 999) {
            WORLD(gHudDisplay).coins = 999;
        }
#else
        if (WORLD(gMarioState)->numCoins > 999) {
            WORLD(gMarioState)->numLives = (s8) 999; //! Wrong variable
        }
#endif

        WORLD(gHudDisplay).stars = WORLD(gMarioState)->numStars;
        WORLD(gHudDisplay).lives = WORLD(gMarioState)->numLives;
        WORLD(gHudDisplay).keys = WORLD(gMarioState)->numKeys;

        if (numHealthWedges > WORLD(gHudDisplay).wedges) {
            play_sound(SOUND_MENU_POWER_METER, WORLD(gGlobalSoundSource));
        }
        WORLD(gHudDisplay).wedges = numHealthWedges;

        if (WORLD(gMarioState)->hurtCounter > 0) {
            WORLD(gHudDisplay).flags |= HUD_DISPLAY_FLAG_EMPHASIZE_POWER;
        } else {
            WORLD(gHudDisplay).flags &= ~HUD_DISPLAY_FLAG_EMPHASIZE_POWER;
        }
    }
}

/**
 * Update objects, HUD, and camera. This update function excludes things like
 * endless staircase, warps, pausing, etc. This is used when entering a painting,
 * presumably to allow painting and camera updating while avoiding triggering the
 * warp twice.
 */
void basic_update(UNUSED s16 *arg) {
    N64_STACK_FRAME(basic_update);
    area_update_objects();
    update_hud_values();

    if (WORLD(gCurrentArea) != NULL) {
        update_camera(WORLD(gCurrentArea)->camera);
    }
}

s32 play_mode_normal(void) {
    N64_STACK_FRAME(play_mode_normal);
    if (WORLD(gCurrDemoInput) != NULL) {
        print_intro_text();
        if (WORLD(gPlayer1Controller)->buttonPressed & END_DEMO) {
            level_trigger_warp(WORLD(gMarioState),
                               WORLD(gCurrLevelNum) == LEVEL_PSS ? WARP_OP_DEMO_END : WARP_OP_DEMO_NEXT);
        } else if (!WORLD(gWarpTransition).isActive && WORLD(sDelayedWarpOp) == WARP_OP_NONE
                   && (WORLD(gPlayer1Controller)->buttonPressed & START_BUTTON)) {
            level_trigger_warp(WORLD(gMarioState), WARP_OP_DEMO_NEXT);
        }
    }

    warp_area();
    check_instant_warp();

    if (WORLD(sTimerRunning) && WORLD(gHudDisplay).timer < 17999) {
        WORLD(gHudDisplay).timer++;
    }

    area_update_objects();
    update_hud_values();

    if (WORLD(gCurrentArea) != NULL) {
        update_camera(WORLD(gCurrentArea)->camera);
    }

    initiate_painting_warp();
    initiate_delayed_warp();

    // If either initiate_painting_warp or initiate_delayed_warp initiated a
    // warp, change play mode accordingly.
    if (WORLD(sCurrPlayMode) == PLAY_MODE_NORMAL) {
        if (WORLD(sWarpDest).type == WARP_TYPE_CHANGE_LEVEL) {
            set_play_mode(PLAY_MODE_CHANGE_LEVEL);
        } else if (WORLD(sTransitionTimer) != 0) {
            set_play_mode(PLAY_MODE_CHANGE_AREA);
        } else if (pressed_pause()) {
            lower_background_noise(1);
#if ENABLE_RUMBLE
            cancel_rumble();
#endif
            WORLD(gCameraMovementFlags) |= CAM_MOVE_PAUSE_SCREEN;
            set_play_mode(PLAY_MODE_PAUSED);
        }
    }

    return 0;
}

s32 play_mode_paused(void) {
    if (WORLD(gMenuOptSelectIndex) == MENU_OPT_NONE) {
        set_menu_mode(MENU_MODE_RENDER_PAUSE_SCREEN);
    } else if (WORLD(gMenuOptSelectIndex) == MENU_OPT_DEFAULT) {
        raise_background_noise(1);
        WORLD(gCameraMovementFlags) &= ~CAM_MOVE_PAUSE_SCREEN;
        set_play_mode(PLAY_MODE_NORMAL);
    } else { // MENU_OPT_EXIT_COURSE
        if (WORLD(gDebugLevelSelect)) {
            fade_into_special_warp(-9, 1);
        } else {
            initiate_warp(LEVEL_CASTLE, 1, 0x1F, 0);
            fade_into_special_warp(0, 0);
            WORLD(gSavedCourseNum) = COURSE_NONE;
        }

        WORLD(gCameraMovementFlags) &= ~CAM_MOVE_PAUSE_SCREEN;
    }

    return 0;
}

/**
 * Debug mode that lets you frame advance by pressing D-pad down. Unfortunately
 * it uses the pause camera, making it basically unusable in most levels.
 */
s32 play_mode_frame_advance(void) {
    N64_STACK_FRAME(play_mode_frame_advance);
    if (WORLD(gPlayer1Controller)->buttonPressed & D_JPAD) {
        WORLD(gCameraMovementFlags) &= ~CAM_MOVE_PAUSE_SCREEN;
        play_mode_normal();
    } else if (WORLD(gPlayer1Controller)->buttonPressed & START_BUTTON) {
        WORLD(gCameraMovementFlags) &= ~CAM_MOVE_PAUSE_SCREEN;
        raise_background_noise(1);
        set_play_mode(PLAY_MODE_NORMAL);
    } else {
        WORLD(gCameraMovementFlags) |= CAM_MOVE_PAUSE_SCREEN;
    }

    return 0;
}

/**
 * Set the transition, which is a period of time after the warp is initiated
 * but before it actually occurs. If updateFunction is not NULL, it will be
 * called each frame during the transition.
 */
void level_set_transition(s16 length, void (*updateFunction)(s16 *)) {
    WORLD(sTransitionTimer) = length;
    WORLD(sTransitionUpdate) = updateFunction;
}

/**
 * Play the transition and then return to normal play mode.
 */
s32 play_mode_change_area(void) {
    N64_STACK_FRAME(play_mode_change_area);
    //! This maybe was supposed to be sTransitionTimer == -1? sTransitionUpdate
    // is never set to -1.
    if (WORLD(sTransitionUpdate) == (void (*)(s16 *)) -1) {
        update_camera(WORLD(gCurrentArea)->camera);
    } else if (WORLD(sTransitionUpdate) != NULL) {
        WORLD(sTransitionUpdate)(&WORLD(sTransitionTimer));
    }

    if (WORLD(sTransitionTimer) > 0) {
        WORLD(sTransitionTimer)--;
    }

    if (WORLD(sTransitionTimer) == 0) {
        WORLD(sTransitionUpdate) = NULL;
        set_play_mode(PLAY_MODE_NORMAL);
    }

    return 0;
}

/**
 * Play the transition and then return to normal play mode.
 */
s32 play_mode_change_level(void) {
    N64_STACK_FRAME(play_mode_change_level);
    if (WORLD(sTransitionUpdate) != NULL) {
        WORLD(sTransitionUpdate)(&WORLD(sTransitionTimer));
    }

    if (--WORLD(sTransitionTimer) == -1) {
        WORLD(gHudDisplay).flags = HUD_DISPLAY_NONE;
        WORLD(sTransitionTimer) = 0;
        WORLD(sTransitionUpdate) = NULL;

        if (WORLD(sWarpDest).type != WARP_TYPE_NOT_WARPING) {
            return WORLD(sWarpDest).levelNum;
        } else {
            return WORLD(D_80339EE0);
        }
    }

    return 0;
}

/**
 * Unused play mode. Doesn't call transition update and doesn't reset transition at the end.
 */
UNUSED static s32 play_mode_unused(void) {
    if (--WORLD(sTransitionTimer) == -1) {
        WORLD(gHudDisplay).flags = HUD_DISPLAY_NONE;

        if (WORLD(sWarpDest).type != WARP_TYPE_NOT_WARPING) {
            return WORLD(sWarpDest).levelNum;
        } else {
            return WORLD(D_80339EE0);
        }
    }

    return 0;
}

s32 update_level(void) {
    N64_STACK_FRAME(update_level);
    s32 changeLevel;

    switch (WORLD(sCurrPlayMode)) {
        case PLAY_MODE_NORMAL:
            changeLevel = play_mode_normal();
            break;
        case PLAY_MODE_PAUSED:
            changeLevel = play_mode_paused();
            break;
        case PLAY_MODE_CHANGE_AREA:
            changeLevel = play_mode_change_area();
            break;
        case PLAY_MODE_CHANGE_LEVEL:
            changeLevel = play_mode_change_level();
            break;
        case PLAY_MODE_FRAME_ADVANCE:
            changeLevel = play_mode_frame_advance();
            break;
    }

    if (changeLevel) {
        reset_volume();
        enable_background_sound();
    }

    return changeLevel;
}

s32 init_level(void) {
    N64_STACK_FRAME(init_level);
    s32 val4 = FALSE;

    set_play_mode(PLAY_MODE_NORMAL);

    WORLD(sDelayedWarpOp) = WARP_OP_NONE;
    WORLD(sTransitionTimer) = 0;
    WORLD(D_80339EE0) = 0;
    WORLD(gHudDisplay).flags = WORLD(gCurrCreditsEntry) == NULL ? HUD_DISPLAY_DEFAULT : HUD_DISPLAY_NONE;
    WORLD(sTimerRunning) = FALSE;

    if (WORLD(sWarpDest).type != WARP_TYPE_NOT_WARPING) {
        if (WORLD(sWarpDest).nodeId >= WARP_NODE_CREDITS_MIN) {
            warp_credits();
        } else {
            warp_level();
        }
    } else {
        if (WORLD(gPlayerSpawnInfos)[0].areaIndex >= 0) {
            load_mario_area();
            init_mario();
        }

        if (WORLD(gCurrentArea) != NULL) {
            reset_camera(WORLD(gCurrentArea)->camera);

            if (WORLD(gCurrDemoInput) != NULL) {
                set_mario_action(WORLD(gMarioState), ACT_IDLE, 0);
            } else if (!WORLD(gDebugLevelSelect)) {
                if (WORLD(gMarioState)->action != ACT_UNINITIALIZED) {
                    if (save_file_exists(WORLD(gCurrSaveFileNum) - 1)) {
                        set_mario_action(WORLD(gMarioState), ACT_IDLE, 0);
                    } else {
                        set_mario_action(WORLD(gMarioState), ACT_INTRO_CUTSCENE, 0);
                        val4 = TRUE;
                    }
                }
            }
        }

        if (val4) {
            play_transition(WARP_TRANSITION_FADE_FROM_COLOR, 0x5A, 0xFF, 0xFF, 0xFF);
        } else {
            play_transition(WARP_TRANSITION_FADE_FROM_STAR, 0x10, 0xFF, 0xFF, 0xFF);
        }

        if (WORLD(gCurrDemoInput) == NULL) {
            set_background_music(WORLD(gCurrentArea)->musicParam, WORLD(gCurrentArea)->musicParam2, 0);
        }
    }
#if ENABLE_RUMBLE
    if (WORLD(gCurrDemoInput) == NULL) {
        cancel_rumble();
    }
#endif

    if (WORLD(gMarioState)->action == ACT_INTRO_CUTSCENE) {
        sound_banks_disable(SEQ_PLAYER_SFX, SOUND_BANKS_DISABLED_DURING_INTRO_CUTSCENE);
    }

    return 1;
}

/**
 * Initialize the current level if initOrUpdate is 0, or update the level if it is 1.
 */
s32 lvl_init_or_update(s16 initOrUpdate, UNUSED s32 unused) {
    N64_STACK_FRAME(lvl_init_or_update);
    s32 result = 0;

    switch (initOrUpdate) {
        case 0:
            result = init_level();
            break;
        case 1:
            result = update_level();
            break;
    }

    return result;
}

s32 lvl_init_from_save_file(UNUSED s16 arg0, s32 levelNum) {
#ifdef VERSION_EU
    s16 language = eu_get_language();
    switch (language) {
        case LANGUAGE_ENGLISH:
            load_segment_decompress(0x19, _translation_en_mio0SegmentRomStart,
                                    _translation_en_mio0SegmentRomEnd);
            break;
        case LANGUAGE_FRENCH:
            load_segment_decompress(0x19, _translation_fr_mio0SegmentRomStart,
                                    _translation_fr_mio0SegmentRomEnd);
            break;
        case LANGUAGE_GERMAN:
            load_segment_decompress(0x19, _translation_de_mio0SegmentRomStart,
                                    _translation_de_mio0SegmentRomEnd);
            break;
    }
#endif
    WORLD(sWarpDest).type = WARP_TYPE_NOT_WARPING;
    WORLD(sDelayedWarpOp) = WARP_OP_NONE;
    WORLD(gNeverEnteredCastle) = !save_file_exists(WORLD(gCurrSaveFileNum) - 1);

    WORLD(gCurrLevelNum) = levelNum;
    WORLD(gCurrCourseNum) = COURSE_NONE;
    WORLD(gSavedCourseNum) = COURSE_NONE;
    WORLD(gCurrCreditsEntry) = NULL;
    WORLD(gSpecialTripleJump) = FALSE;

    init_mario_from_save_file();
    disable_warp_checkpoint();
    save_file_move_cap_to_default_location();
    select_mario_cam_mode();
    set_yoshi_as_not_dead();

    return levelNum;
}

s32 lvl_set_current_level(UNUSED s16 arg0, s32 levelNum) {
    s32 warpCheckpointActive = WORLD(sWarpCheckpointActive);

    WORLD(sWarpCheckpointActive) = FALSE;
    WORLD(gCurrLevelNum) = levelNum;
    WORLD(gCurrCourseNum) = WORLD(gLevelToCourseNumTable)[levelNum - 1];

    if (WORLD(gCurrDemoInput) != NULL || WORLD(gCurrCreditsEntry) != NULL || WORLD(gCurrCourseNum) == COURSE_NONE) {
        return 0;
    }

    if (WORLD(gCurrLevelNum) != LEVEL_BOWSER_1 && WORLD(gCurrLevelNum) != LEVEL_BOWSER_2
        && WORLD(gCurrLevelNum) != LEVEL_BOWSER_3) {
        WORLD(gMarioState)->numCoins = 0;
        WORLD(gHudDisplay).coins = 0;
        WORLD(gCurrCourseStarFlags) =
            save_file_get_star_flags(WORLD(gCurrSaveFileNum) - 1, COURSE_NUM_TO_INDEX(WORLD(gCurrCourseNum)));
    }

    if (WORLD(gSavedCourseNum) != WORLD(gCurrCourseNum)) {
        WORLD(gSavedCourseNum) = WORLD(gCurrCourseNum);
        nop_change_course();
        disable_warp_checkpoint();
    }

    if (WORLD(gCurrCourseNum) > COURSE_STAGES_MAX || warpCheckpointActive) {
        return 0;
    }

    if (WORLD(gDebugLevelSelect) && !WORLD(gShowProfiler)) {
        return 0;
    }

    return 1;
}

/**
 * Play the "thank you so much for to playing my game" sound.
 */
s32 lvl_play_the_end_screen_sound(UNUSED s16 arg0, UNUSED s32 arg1) {
    play_sound(SOUND_MENU_THANK_YOU_PLAYING_MY_GAME, WORLD(gGlobalSoundSource));
    return 1;
}

// Library: its variables' addresses (tools/state/types.py).
#include "pointers/game/src/game/level_update.c.inc.c"
