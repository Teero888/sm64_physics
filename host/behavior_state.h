#ifndef SM64_PHYSICS_BEHAVIOR_STATE_H
#define SM64_PHYSICS_BEHAVIOR_STATE_H
#include "behavior_data.h"
#include "engine/behavior_script.h"
#include "game/area.h"
#include "game/behavior_actions.h"
#include "game/game_init.h"
#include "game/mario.h"
#include "game/memory.h"
#include "game/obj_behaviors_2.h"
#include "host/objects_state.h"
typedef void (*NativeBhvFunc)(void);
typedef s32 (*BhvCommandProc)(void);
#define gRandomSeed16 (sm64_active_objects->random_seed)
#define gCurBhvCommand (sm64_active_objects->command)
#define gGlobalTimer (sm64_active_objects->global_timer)
#define gLoadedGraphNodes (sm64_active_objects->models)
/* Original bytecode argument macros from behavior_script.c. Bytecode contains
 * native pointers; host decoding must resolve addresses before execution. */
#define BHV_CMD_GET_1ST_U8(index)  (u8)((gCurBhvCommand[index] >> 24) & 0xFF)
#define BHV_CMD_GET_2ND_U8(index)  (u8)((gCurBhvCommand[index] >> 16) & 0xFF)
#define BHV_CMD_GET_3RD_U8(index)  (u8)((gCurBhvCommand[index] >> 8) & 0xFF)
#define BHV_CMD_GET_4TH_U8(index)  (u8)((gCurBhvCommand[index]) & 0xFF)
#define BHV_CMD_GET_1ST_S16(index) (s16)(gCurBhvCommand[index] >> 16)
#define BHV_CMD_GET_2ND_S16(index) (s16)(gCurBhvCommand[index] & 0xFFFF)
#define BHV_CMD_GET_U32(index) (u32)(gCurBhvCommand[index])
#define BHV_CMD_GET_VPTR(index) (void *)(gCurBhvCommand[index])
#define BHV_CMD_GET_ADDR_OF_CMD(index) (uintptr_t)(&gCurBhvCommand[index])
#endif
