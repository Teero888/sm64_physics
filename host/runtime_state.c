#include "types.h"
#include "game/area.h"
#include "game/camera.h"
#include "game/game_init.h"
#include "game/level_update.h"
#include "game/mario_misc.h"
#include "game/save_file.h"
#include "game/object_list_processor.h"
#include "PR/gbi.h"
#include "engine/graph_node.h"

struct MarioState gMarioStates[2];
struct MarioState *gMarioState = &gMarioStates[0];
struct PlayerCameraState gPlayerCameraState[2];
struct MarioBodyState gBodyStates[2];
struct Controller gControllers[3];
struct DmaHandlerList gMarioAnimsBuf;
struct SpawnInfo gPlayerSpawnInfos[1];
struct HudDisplay gHudDisplay;

struct GraphNodeObject *gCurGraphNodeObject = NULL;
struct GraphNodeCamera *gCurGraphNodeCamera = NULL;
struct GraphNodeHeldObject *gCurGraphNodeHeldObject = NULL;
static struct GraphNode *sEmptyGraphNodes[256];
struct GraphNode **gLoadedGraphNodes = sEmptyGraphNodes;
struct Object *gSecondCameraFocus = NULL;
struct CreditsEntry *gCurrCreditsEntry = NULL;
s32 gDialogResponse = 0;

s16 gCurrSaveFileNum = 1;
u8 gSpecialTripleJump = 0;
s16 gCurrAreaIndex = 1;
struct Area *gCurrentArea = NULL;
u32 gAudioRandom = 0;
u32 gGlobalTimer = 0;

struct Camera *gCamera = NULL;
s16 gCameraMovementFlags = 0;
u16 gAreaUpdateCounter = 0;
s16 gSaveOptSelectIndex = 0;
s8 gNeverEnteredCastle = 0;
u8 gLastCompletedStarNum = 0;
u8 gLastCompletedCourseNum = 0;
Vec3f gGlobalSoundSource = {0};
struct Object *gCutsceneFocus = NULL;
f32 gPaintingMarioYEntry = 0.0f;

void print_credits_str_ascii(s16 x, s16 y, const char *str) {
    (void)x; (void)y; (void)str;
}

void dl_rgba16_begin_cutscene_msg_fade(void) {}
void dl_rgba16_stop_cutscene_msg_fade(void) {}

void debug_unknown_level_select_check(void) {}
u64 osClockRate = 62500000;
s16 gCurrCourseNum = 0;
const Collision warp_pipe_seg3_collision_03009AC8[] = { 0 };

u16 level_control_timer(s32 timerOp) {
    (void)timerOp;
    return 0;
}

void fade_into_special_warp(u32 arg, u32 color) {
    (void)arg; (void)color;
}

s32 trigger_cutscene_dialog(s32 trigger) {
    (void)trigger;
    return 0;
}

void create_dialog_inverted_box(s16 dialog) {
    (void)dialog;
}

void set_menu_mode(s16 mode) {
    (void)mode;
}

f32 camera_approach_f32_symmetric(f32 value, f32 target, f32 increment) {
    (void)increment;
    return target;
}

void play_transition(s16 transType, s16 time, u8 red, u8 green, u8 blue) {
    (void)transType; (void)time; (void)red; (void)green; (void)blue;
}

void set_cutscene_message(s16 xOffset, s16 yOffset, s16 msgIndex, s16 msgDuration) {
    (void)xOffset; (void)yOffset; (void)msgIndex; (void)msgDuration;
}

void override_viewport_and_clip(Vp *a, Vp *b, u8 c, u8 d, u8 e) {
    (void)a; (void)b; (void)c; (void)d; (void)e;
}

void reset_cutscene_msg_fade(void) {}

void *alloc_display_list(u32 size) {
    (void)size;
    return NULL;
}

void print_debug_top_down_objectinfo(const char *str, s32 number) {
    (void)str; (void)number;
}

void create_dialog_box(s16 dialog) {
    (void)dialog;
}

void create_dialog_box_with_var(s16 dialog, s32 dialogVar) {
    (void)dialog; (void)dialogVar;
}

void create_dialog_box_with_response(s16 dialog) {
    (void)dialog;
}

s16 get_dialog_id(void) {
    return -1;
}

s16 cutscene_object_with_dialog(u8 cutscene, struct Object *o, s16 dialogID) {
    (void)cutscene; (void)o; (void)dialogID;
    return 1;
}

s16 cutscene_object_without_dialog(u8 cutscene, struct Object *o) {
    (void)cutscene; (void)o;
    return 1;
}

s16 cutscene_object(u8 cutscene, struct Object *o) {
    (void)cutscene; (void)o;
    return 1;
}

void spawn_default_star(f32 homeX, f32 homeY, f32 homeZ) {
    (void)homeX; (void)homeY; (void)homeZ;
}

s16 level_trigger_warp(struct MarioState *m, s32 warpOp) {
    (void)m; (void)warpOp;
    return 0;
}

s16 gCurrLevelNum = 1;
s16 gShowDebugText = 0;
struct Controller *gPlayer1Controller = &gControllers[0];
struct SpawnInfo *gMarioSpawnInfo = &gPlayerSpawnInfos[0];

void print_text_fmt_int(s32 x, s32 y, const char *str, s32 n) {
    (void)x; (void)y; (void)str; (void)n;
}

void play_infinite_stairs_music(void) {
}

void load_level_init_text(u32 arg) {
    (void)arg;
}

void spawn_wind_particles(s16 pitch, s16 yaw) {
    (void)pitch; (void)yaw;
}

void bhv_spawn_star_no_level_exit(u32 arg) {
    (void)arg;
}

void spawn_mist_particles_variable(s32 count, s32 offsetY, f32 size) {
    (void)count; (void)offsetY; (void)size;
}

void spawn_triangle_break_particles(s16 numTris, s16 triModel, f32 triSize, s16 triAnimState) {
    (void)numTris; (void)triModel; (void)triSize; (void)triAnimState;
}

#define DUMMY_BHV { ((uintptr_t)0x00 << 24) | ((uintptr_t)OBJ_LIST_UNIMPORTANT << 16), ((uintptr_t)0x0a << 24) }

const BehaviorScript bhvMistParticleSpawner[] = DUMMY_BHV;
const BehaviorScript bhvVertStarParticleSpawner[] = DUMMY_BHV;
const BehaviorScript bhvHorStarParticleSpawner[] = DUMMY_BHV;
const BehaviorScript bhvSparkleParticleSpawner[] = DUMMY_BHV;
const BehaviorScript bhvBubbleParticleSpawner[] = DUMMY_BHV;
const BehaviorScript bhvWaterSplash[] = DUMMY_BHV;
const BehaviorScript bhvIdleWaterWave[] = DUMMY_BHV;
const BehaviorScript bhvPlungeBubble[] = DUMMY_BHV;
const BehaviorScript bhvWaveTrail[] = DUMMY_BHV;
const BehaviorScript bhvFireParticleSpawner[] = DUMMY_BHV;
const BehaviorScript bhvShallowWaterWave[] = DUMMY_BHV;
const BehaviorScript bhvShallowWaterSplash[] = DUMMY_BHV;
const BehaviorScript bhvLeafParticleSpawner[] = DUMMY_BHV;
const BehaviorScript bhvSnowParticleSpawner[] = DUMMY_BHV;
const BehaviorScript bhvBreathParticleSpawner[] = DUMMY_BHV;
const BehaviorScript bhvDirtParticleSpawner[] = DUMMY_BHV;
const BehaviorScript bhvMistCircParticleSpawner[] = DUMMY_BHV;
const BehaviorScript bhvTriangleParticleSpawner[] = DUMMY_BHV;
const BehaviorScript bhvNormalCap[] = DUMMY_BHV;
const BehaviorScript bhvBobomb[] = DUMMY_BHV;
const BehaviorScript bhvStarKeyCollectionPuffSpawner[] = DUMMY_BHV;
const BehaviorScript bhvJumpingBox[] = DUMMY_BHV;
const BehaviorScript bhvCelebrationStar[] = DUMMY_BHV;
const BehaviorScript bhvEndToad[] = DUMMY_BHV;
const BehaviorScript bhvEndPeach[] = DUMMY_BHV;
const BehaviorScript bhvStaticObject[] = DUMMY_BHV;
const BehaviorScript bhvBowserKeyCourseExit[] = DUMMY_BHV;
const BehaviorScript bhvBowserKeyUnlockDoor[] = DUMMY_BHV;
const BehaviorScript bhvSparkleSpawn[] = DUMMY_BHV;
const BehaviorScript bhvUnlockDoorStar[] = DUMMY_BHV;
const BehaviorScript bhvKoopaShellUnderwater[] = DUMMY_BHV;
const BehaviorScript bhvTree[] = DUMMY_BHV;
const BehaviorScript bhvGiantPole[] = DUMMY_BHV;
const BehaviorScript bhvMetalCap[] = DUMMY_BHV;
const BehaviorScript bhvWingCap[] = DUMMY_BHV;
const BehaviorScript bhvVanishCap[] = DUMMY_BHV;
const BehaviorScript bhvBowser[] = DUMMY_BHV;
const BehaviorScript bhvSpawnedStarNoLevelExit[] = DUMMY_BHV;
const BehaviorScript bhvSpawnedBlueCoin[] = DUMMY_BHV;
const BehaviorScript bhvBlueCoinJumping[] = DUMMY_BHV;
const BehaviorScript bhvWhitePuffExplosion[] = DUMMY_BHV;
const BehaviorScript bhvSingleCoinGetsSpawned[] = DUMMY_BHV;
const BehaviorScript bhvSoundSpawner[] = DUMMY_BHV;
