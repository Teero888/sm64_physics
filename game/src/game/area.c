#include <PR/ultratypes.h>

#include "prevent_bss_reordering.h"
#include "area.h"
#include "sm64.h"
#include "gfx_dimensions.h"
#include "behavior_data.h"
#include "game_init.h"
#include "object_list_processor.h"
#include "engine/surface_load.h"
#include "ingame_menu.h"
#include "screen_transition.h"
#include "mario.h"
#include "mario_actions_cutscene.h"
#include "print.h"
#include "hud.h"
#include "audio/external.h"
#include "area.h"
#include "rendering_graph_node.h"
#include "level_update.h"
#include "engine/geo_layout.h"
#include "save_file.h"
#include "level_table.h"
#include "dialog_ids.h"

struct SpawnInfo gPlayerSpawnInfos[1];
struct GraphNode *D_8033A160[0x100];
struct Area gAreaData[8];

struct WarpTransition gWarpTransition;

s16 gCurrCourseNum;
s16 gCurrActNum;
s16 gCurrAreaIndex;
s16 gSavedCourseNum;
s16 gMenuOptSelectIndex;
s16 gSaveOptSelectIndex;

struct SpawnInfo *gMarioSpawnInfo = &gPlayerSpawnInfos[0];
struct GraphNode **gLoadedGraphNodes = D_8033A160;
struct Area *gAreas = gAreaData;
struct Area *gCurrentArea = NULL;
struct CreditsEntry *gCurrCreditsEntry = NULL;
Vp *D_8032CE74 = NULL;
Vp *D_8032CE78 = NULL;
s16 gWarpTransDelay = 0;
u32 gFBSetColor = 0;
u32 gWarpTransFBSetColor = 0;
u8 gWarpTransRed = 0;
u8 gWarpTransGreen = 0;
u8 gWarpTransBlue = 0;
s16 gCurrSaveFileNum = 1;
s16 gCurrLevelNum = LEVEL_MIN;

/*
 * The following two tables are used in get_mario_spawn_type() to determine spawn type
 * from warp behavior.
 * When looping through sWarpBhvSpawnTable, if the behavior function in the table matches
 * the spawn behavior executed, the index of that behavior is used with sSpawnTypeFromWarpBhv
*/

const BehaviorScript *sWarpBhvSpawnTable[] = {
    bhvDoorWarp,                bhvStar,                   bhvExitPodiumWarp,          bhvWarp,
    bhvWarpPipe,                bhvFadingWarp,             bhvInstantActiveWarp,       bhvAirborneWarp,
    bhvHardAirKnockBackWarp,    bhvSpinAirborneCircleWarp, bhvDeathWarp,               bhvSpinAirborneWarp,
    bhvFlyingWarp,              bhvSwimmingWarp,           bhvPaintingStarCollectWarp, bhvPaintingDeathWarp,
    bhvAirborneStarCollectWarp, bhvAirborneDeathWarp,      bhvLaunchStarCollectWarp,   bhvLaunchDeathWarp,
};

u8 sSpawnTypeFromWarpBhv[] = {
    MARIO_SPAWN_DOOR_WARP,             MARIO_SPAWN_UNKNOWN_02,           MARIO_SPAWN_UNKNOWN_03,            MARIO_SPAWN_UNKNOWN_03,
    MARIO_SPAWN_UNKNOWN_03,            MARIO_SPAWN_TELEPORT,             MARIO_SPAWN_INSTANT_ACTIVE,        MARIO_SPAWN_AIRBORNE,
    MARIO_SPAWN_HARD_AIR_KNOCKBACK,    MARIO_SPAWN_SPIN_AIRBORNE_CIRCLE, MARIO_SPAWN_DEATH,                 MARIO_SPAWN_SPIN_AIRBORNE,
    MARIO_SPAWN_FLYING,                MARIO_SPAWN_SWIMMING,             MARIO_SPAWN_PAINTING_STAR_COLLECT, MARIO_SPAWN_PAINTING_DEATH,
    MARIO_SPAWN_AIRBORNE_STAR_COLLECT, MARIO_SPAWN_AIRBORNE_DEATH,       MARIO_SPAWN_LAUNCH_STAR_COLLECT,   MARIO_SPAWN_LAUNCH_DEATH,
};

Vp D_8032CF00 = { {
    { 640, 480, 511, 0 },
    { 640, 480, 511, 0 },
} };

#ifdef VERSION_EU
const char *gNoControllerMsg[] = {
    "NO CONTROLLER",
    "MANETTE DEBRANCHEE",
    "CONTROLLER FEHLT",
};
#endif

void override_viewport_and_clip(Vp *a, Vp *b, u8 c, u8 d, u8 e) {
    u16 sp6 = ((c >> 3) << 11) | ((d >> 3) << 6) | ((e >> 3) << 1) | 1;

    WORLD(gFBSetColor) = (sp6 << 16) | sp6;
    WORLD(D_8032CE74) = a;
    WORLD(D_8032CE78) = b;
}

void set_warp_transition_rgb(u8 red, u8 green, u8 blue) {
    u16 warpTransitionRGBA16 = ((red >> 3) << 11) | ((green >> 3) << 6) | ((blue >> 3) << 1) | 1;

    WORLD(gWarpTransFBSetColor) = (warpTransitionRGBA16 << 16) | warpTransitionRGBA16;
    WORLD(gWarpTransRed) = red;
    WORLD(gWarpTransGreen) = green;
    WORLD(gWarpTransBlue) = blue;
}

void print_intro_text(void) {
#ifdef VERSION_CN
    u8 sp18[] = { 0xB0, 0x00 }; // TODO: iQue colorful text
#endif
#ifdef VERSION_EU
    s32 language = eu_get_language();
#endif
    if ((WORLD(gGlobalTimer) & 31) < 20) {
        if (WORLD(gControllerBits) == 0) {
#ifdef VERSION_EU
            print_text_centered(SCREEN_WIDTH / 2, 20, gNoControllerMsg[language]);
#else
            print_text_centered(SCREEN_WIDTH / 2, 20, "NO CONTROLLER");
#endif
        } else {
#ifdef VERSION_EU
            print_text(20, 20, "START");
#else
#ifdef VERSION_CN
            print_text_centered(60, 38, (char *) sp18);
#else
            print_text_centered(60, 38, "PRESS");
#endif
            print_text_centered(60, 20, "START");
#endif
        }
    }
}

u32 get_mario_spawn_type(struct Object *o) {
    s32 i;
    const BehaviorScript *behavior = virtual_to_segmented(0x13, o->behavior);

    for (i = 0; i < 20; i++) {
        if (behavior == WORLD(sWarpBhvSpawnTable)[i]) {
            return WORLD(sSpawnTypeFromWarpBhv)[i];
        }
    }
    return 0;
}

struct ObjectWarpNode *area_get_warp_node(u8 id) {
    struct ObjectWarpNode *node = NULL;

    for (node = WORLD(gCurrentArea)->warpNodes; node != NULL; node = node->next) {
        if (node->node.id == id) {
            break;
        }
    }
    return node;
}

struct ObjectWarpNode *area_get_warp_node_from_params(struct Object *o) {
    u8 id = (o->oBhvParams & 0x00FF0000) >> 16;

    return area_get_warp_node(id);
}

void load_obj_warp_nodes(void) {
    struct ObjectWarpNode *sp24;
    struct Object *sp20 = (struct Object *) WORLD(gObjParentGraphNode).children;

    do {
        struct Object *sp1C = sp20;

        if (sp1C->activeFlags != ACTIVE_FLAG_DEACTIVATED && get_mario_spawn_type(sp1C) != 0) {
            sp24 = area_get_warp_node_from_params(sp1C);
            if (sp24 != NULL) {
                sp24->object = sp1C;
            }
        }
    } while ((sp20 = (struct Object *) sp20->header.gfx.node.next)
             != (struct Object *) WORLD(gObjParentGraphNode).children);
}

void clear_areas(void) {
    s32 i;

    WORLD(gCurrentArea) = NULL;
    WORLD(gWarpTransition).isActive = FALSE;
    WORLD(gWarpTransition).pauseRendering = FALSE;
    WORLD(gMarioSpawnInfo)->areaIndex = -1;

    for (i = 0; i < 8; i++) {
        WORLD(gAreaData)[i].index = i;
        WORLD(gAreaData)[i].flags = 0;
        WORLD(gAreaData)[i].terrainType = 0;
        WORLD(gAreaData)[i].unk04 = NULL;
        WORLD(gAreaData)[i].terrainData = NULL;
        WORLD(gAreaData)[i].surfaceRooms = NULL;
        WORLD(gAreaData)[i].macroObjects = NULL;
        WORLD(gAreaData)[i].warpNodes = NULL;
        WORLD(gAreaData)[i].paintingWarpNodes = NULL;
        WORLD(gAreaData)[i].instantWarps = NULL;
        WORLD(gAreaData)[i].objectSpawnInfos = NULL;
        WORLD(gAreaData)[i].camera = NULL;
        WORLD(gAreaData)[i].unused = NULL;
        WORLD(gAreaData)[i].whirlpools[0] = NULL;
        WORLD(gAreaData)[i].whirlpools[1] = NULL;
        WORLD(gAreaData)[i].dialog[0] = DIALOG_NONE;
        WORLD(gAreaData)[i].dialog[1] = DIALOG_NONE;
        WORLD(gAreaData)[i].musicParam = 0;
        WORLD(gAreaData)[i].musicParam2 = 0;
    }
}

void clear_area_graph_nodes(void) {
    s32 i;

    if (WORLD(gCurrentArea) != NULL) {
        geo_call_global_function_nodes(&WORLD(gCurrentArea)->unk04->node, GEO_CONTEXT_AREA_UNLOAD);
        WORLD(gCurrentArea) = NULL;
        WORLD(gWarpTransition).isActive = FALSE;
    }

    for (i = 0; i < 8; i++) {
        if (WORLD(gAreaData)[i].unk04 != NULL) {
            geo_call_global_function_nodes(&WORLD(gAreaData)[i].unk04->node, GEO_CONTEXT_AREA_INIT);
            WORLD(gAreaData)[i].unk04 = NULL;
        }
    }
}

void load_area(s32 index) {
    if (WORLD(gCurrentArea) == NULL && WORLD(gAreaData)[index].unk04 != NULL) {
        WORLD(gCurrentArea) = &WORLD(gAreaData)[index];
        WORLD(gCurrAreaIndex) = WORLD(gCurrentArea)->index;

        if (WORLD(gCurrentArea)->terrainData != NULL) {
            load_area_terrain(index, WORLD(gCurrentArea)->terrainData, WORLD(gCurrentArea)->surfaceRooms,
                              WORLD(gCurrentArea)->macroObjects);
        }

        if (WORLD(gCurrentArea)->objectSpawnInfos != NULL) {
            spawn_objects_from_info(0, WORLD(gCurrentArea)->objectSpawnInfos);
        }

        load_obj_warp_nodes();
        geo_call_global_function_nodes(&WORLD(gCurrentArea)->unk04->node, GEO_CONTEXT_AREA_LOAD);
    }
}

void unload_area(void) {
    if (WORLD(gCurrentArea) != NULL) {
        unload_objects_from_area(0, WORLD(gCurrentArea)->index);
        geo_call_global_function_nodes(&WORLD(gCurrentArea)->unk04->node, GEO_CONTEXT_AREA_UNLOAD);

        WORLD(gCurrentArea)->flags = 0;
        WORLD(gCurrentArea) = NULL;
        WORLD(gWarpTransition).isActive = FALSE;
    }
}

void load_mario_area(void) {
    stop_sounds_in_continuous_banks();
    load_area(WORLD(gMarioSpawnInfo)->areaIndex);

    if (WORLD(gCurrentArea)->index == WORLD(gMarioSpawnInfo)->areaIndex) {
        WORLD(gCurrentArea)->flags |= 0x01;
        spawn_objects_from_info(0, WORLD(gMarioSpawnInfo));
    }
}

void unload_mario_area(void) {
    if (WORLD(gCurrentArea) != NULL && (WORLD(gCurrentArea)->flags & 0x01)) {
        unload_objects_from_area(0, WORLD(gMarioSpawnInfo)->activeAreaIndex);

        WORLD(gCurrentArea)->flags &= ~0x01;
        if (WORLD(gCurrentArea)->flags == 0) {
            unload_area();
        }
    }
}

void change_area(s32 index) {
    s32 areaFlags = WORLD(gCurrentArea)->flags;

    if (WORLD(gCurrAreaIndex) != index) {
        unload_area();
        load_area(index);

        WORLD(gCurrentArea)->flags = areaFlags;
        WORLD(gMarioObject)->oActiveParticleFlags = 0;
    }

    if (areaFlags & 0x01) {
        WORLD(gMarioObject)->header.gfx.areaIndex = index, WORLD(gMarioSpawnInfo)->areaIndex = index;
    }
}

void area_update_objects(void) {
    WORLD(gAreaUpdateCounter)++;
    update_objects(0);
}

/*
 * Sets up the information needed to play a warp transition, including the
 * transition type, time in frames, and the RGB color that will fill the screen.
 */
void play_transition(s16 transType, s16 time, u8 red, u8 green, u8 blue) {
    WORLD(gWarpTransition).isActive = TRUE;
    WORLD(gWarpTransition).type = transType;
    WORLD(gWarpTransition).time = time;
    WORLD(gWarpTransition).pauseRendering = FALSE;

    // The lowest bit of transType determines if the transition is fading in or out.
    if (transType & 1) {
        set_warp_transition_rgb(red, green, blue);
    } else {
        red = WORLD(gWarpTransRed), green = WORLD(gWarpTransGreen), blue = WORLD(gWarpTransBlue);
    }

    if (transType < 8) { // if transition is RGB
        WORLD(gWarpTransition).data.red = red;
        WORLD(gWarpTransition).data.green = green;
        WORLD(gWarpTransition).data.blue = blue;
    } else { // if transition is textured
        WORLD(gWarpTransition).data.red = red;
        WORLD(gWarpTransition).data.green = green;
        WORLD(gWarpTransition).data.blue = blue;

        // Both the start and end textured transition are always located in the middle of the screen.
        // If you really wanted to, you could place the start at one corner and the end at
        // the opposite corner. This will make the transition image look like it is moving
        // across the screen.
        WORLD(gWarpTransition).data.startTexX = SCREEN_WIDTH / 2;
        WORLD(gWarpTransition).data.startTexY = SCREEN_HEIGHT / 2;
        WORLD(gWarpTransition).data.endTexX = SCREEN_WIDTH / 2;
        WORLD(gWarpTransition).data.endTexY = SCREEN_HEIGHT / 2;

        WORLD(gWarpTransition).data.texTimer = 0;

        if (transType & 1) { // Is the image fading in?
            WORLD(gWarpTransition).data.startTexRadius = GFX_DIMENSIONS_FULL_RADIUS;
            if (transType >= 0x0F) {
                WORLD(gWarpTransition).data.endTexRadius = 16;
            } else {
                WORLD(gWarpTransition).data.endTexRadius = 0;
            }
        } else { // The image is fading out. (Reverses start & end circles)
            if (transType >= 0x0E) {
                WORLD(gWarpTransition).data.startTexRadius = 16;
            } else {
                WORLD(gWarpTransition).data.startTexRadius = 0;
            }
            WORLD(gWarpTransition).data.endTexRadius = GFX_DIMENSIONS_FULL_RADIUS;
        }
    }
}

/*
 * Sets up the information needed to play a warp transition, including the
 * transition type, time in frames, and the RGB color that will fill the screen.
 * The transition will play only after a number of frames specified by 'delay'
 */
void play_transition_after_delay(s16 transType, s16 time, u8 red, u8 green, u8 blue, s16 delay) {
    WORLD(gWarpTransDelay) = delay; // Number of frames to delay playing the transition.
    play_transition(transType, time, red, green, blue);
}

void render_game(void) {
    if (WORLD(gCurrentArea) != NULL && !WORLD(gWarpTransition).pauseRendering) {
        geo_process_root(WORLD(gCurrentArea)->unk04, WORLD(D_8032CE74), WORLD(D_8032CE78), WORLD(gFBSetColor));

        gSPViewport(WORLD(gDisplayListHead)++, VIRTUAL_TO_PHYSICAL(&WORLD(D_8032CF00)));

        gDPSetScissor(WORLD(gDisplayListHead)++, G_SC_NON_INTERLACE, 0, BORDER_HEIGHT, SCREEN_WIDTH,
                      SCREEN_HEIGHT - BORDER_HEIGHT);
        render_hud();

        gDPSetScissor(WORLD(gDisplayListHead)++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        render_text_labels();
        do_cutscene_handler();
        print_displaying_credits_entry();

        gDPSetScissor(WORLD(gDisplayListHead)++, G_SC_NON_INTERLACE, 0, BORDER_HEIGHT, SCREEN_WIDTH,
                      SCREEN_HEIGHT - BORDER_HEIGHT);
        WORLD(gMenuOptSelectIndex) = render_menus_and_dialogs();
        if (WORLD(gMenuOptSelectIndex) != MENU_OPT_NONE) {
            WORLD(gSaveOptSelectIndex) = WORLD(gMenuOptSelectIndex);
        }

        if (WORLD(D_8032CE78) != NULL) {
            make_viewport_clip_rect(WORLD(D_8032CE78));
        } else {
            gDPSetScissor(WORLD(gDisplayListHead)++, G_SC_NON_INTERLACE, 0, BORDER_HEIGHT, SCREEN_WIDTH,
                          SCREEN_HEIGHT - BORDER_HEIGHT);
        }

        if (WORLD(gWarpTransition).isActive) {
            if (WORLD(gWarpTransDelay) == 0) {
                WORLD(gWarpTransition).isActive = !render_screen_transition(0, WORLD(gWarpTransition).type, WORLD(gWarpTransition).time,
                                                                     &WORLD(gWarpTransition).data);
                if (!WORLD(gWarpTransition).isActive) {
                    if (WORLD(gWarpTransition).type & 1) {
                        WORLD(gWarpTransition).pauseRendering = TRUE;
                    } else {
                        set_warp_transition_rgb(0, 0, 0);
                    }
                }
            } else {
                WORLD(gWarpTransDelay)--;
            }
        }
    } else {
        render_text_labels();
        if (WORLD(D_8032CE78) != NULL) {
            clear_viewport(WORLD(D_8032CE78), WORLD(gWarpTransFBSetColor));
        } else {
            clear_framebuffer(WORLD(gWarpTransFBSetColor));
        }
    }

    WORLD(D_8032CE74) = NULL;
    WORLD(D_8032CE78) = NULL;
}
