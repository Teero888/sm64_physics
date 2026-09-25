#include <ultra64.h>
#include <stdio.h>

#include "sm64.h"
#include "audio/external.h"
#include "game_init.h"
#include "memory.h"
#include "sound_init.h"
#include "profiler.h"
#include "buffers/buffers.h"
#include "segments.h"
#include "segment_symbols.h"
#include "main.h"
#include "rumble_init.h"

// Message IDs
#define MESG_SP_COMPLETE 100
#define MESG_DP_COMPLETE 101
#define MESG_VI_VBLANK 102
#define MESG_START_GFX_SPTASK 103
#define MESG_NMI_REQUEST 104

OSThread D_80339210; // unused?
OSThread gIdleThread;
OSThread gMainThread;
OSThread gGameLoopThread;
OSThread gSoundThread;

OSIoMesg gDmaIoMesg;
OSMesg gMainReceivedMesg;

OSMesgQueue gDmaMesgQueue;
OSMesgQueue gSIEventMesgQueue;
OSMesgQueue gPIMesgQueue;
OSMesgQueue gIntrMesgQueue;
OSMesgQueue gSPTaskMesgQueue;

OSMesg gDmaMesgBuf[1];
OSMesg gPIMesgBuf[32];
OSMesg gSIEventMesgBuf[1];
OSMesg gIntrMesgBuf[16];
OSMesg gUnknownMesgBuf[16];

struct VblankHandler *gVblankHandler1 = NULL;
struct VblankHandler *gVblankHandler2 = NULL;
struct SPTask *gActiveSPTask = NULL;
struct SPTask *sCurrentAudioSPTask = NULL;
struct SPTask *sCurrentDisplaySPTask = NULL;
struct SPTask *sNextAudioSPTask = NULL;
struct SPTask *sNextDisplaySPTask = NULL;
s8 sAudioEnabled = TRUE;
u32 gNumVblanks = 0;
s8 gResetTimer = 0;
s8 gNmiResetBarsTimer = 0;
s8 gDebugLevelSelect = FALSE;
s8 D_8032C650 = 0;

s8 gShowProfiler = FALSE;
s8 gShowDebugText = FALSE;

// unused
void handle_debug_key_sequences(void) {
    static u16 sProfilerKeySequence[] = {
        U_JPAD, U_JPAD, D_JPAD, D_JPAD, L_JPAD, R_JPAD, L_JPAD, R_JPAD
    };
    static u16 sDebugTextKeySequence[] = { D_JPAD, D_JPAD, U_JPAD, U_JPAD,
                                           L_JPAD, R_JPAD, L_JPAD, R_JPAD };
    static s16 sProfilerKey = 0;
    static s16 sDebugTextKey = 0;
    if (WORLD(gPlayer3Controller)->buttonPressed != 0) {
        if (WORLD(sProfilerKeySequence)[WORLD(sProfilerKey)++] == WORLD(gPlayer3Controller)->buttonPressed) {
            if (WORLD(sProfilerKey) == ARRAY_COUNT(WORLD(sProfilerKeySequence))) {
                WORLD(sProfilerKey) = 0, WORLD(gShowProfiler) ^= 1;
            }
        } else {
            WORLD(sProfilerKey) = 0;
        }

        if (WORLD(sDebugTextKeySequence)[WORLD(sDebugTextKey)++] == WORLD(gPlayer3Controller)->buttonPressed) {
            if (WORLD(sDebugTextKey) == ARRAY_COUNT(WORLD(sDebugTextKeySequence))) {
                WORLD(sDebugTextKey) = 0, WORLD(gShowDebugText) ^= 1;
            }
        } else {
            WORLD(sDebugTextKey) = 0;
        }
    }
}

void unknown_main_func(void) {
    // uninitialized
    OSTime time;
    u32 b;
#ifdef AVOID_UB
    time = 0;
    b = 0;
#endif

    osSetTime(time);
    osMapTLB(0, b, NULL, 0, 0, 0);
    osUnmapTLBAll();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnonnull"
    sprintf(NULL, NULL);
#pragma GCC diagnostic pop
}

void stub_main_1(void) {
}

void stub_main_2(void) {
}

void stub_main_3(void) {
}

void setup_mesg_queues(void) {
    osCreateMesgQueue(&WORLD(gDmaMesgQueue), WORLD(gDmaMesgBuf), ARRAY_COUNT(WORLD(gDmaMesgBuf)));
    osCreateMesgQueue(&WORLD(gSIEventMesgQueue), WORLD(gSIEventMesgBuf), ARRAY_COUNT(WORLD(gSIEventMesgBuf)));
    osSetEventMesg(OS_EVENT_SI, &WORLD(gSIEventMesgQueue), NULL);

    osCreateMesgQueue(&WORLD(gSPTaskMesgQueue), WORLD(gUnknownMesgBuf), ARRAY_COUNT(WORLD(gUnknownMesgBuf)));
    osCreateMesgQueue(&WORLD(gIntrMesgQueue), WORLD(gIntrMesgBuf), ARRAY_COUNT(WORLD(gIntrMesgBuf)));
    osViSetEvent(&WORLD(gIntrMesgQueue), (OSMesg) MESG_VI_VBLANK, 1);

    osSetEventMesg(OS_EVENT_SP, &WORLD(gIntrMesgQueue), (OSMesg) MESG_SP_COMPLETE);
    osSetEventMesg(OS_EVENT_DP, &WORLD(gIntrMesgQueue), (OSMesg) MESG_DP_COMPLETE);
    osSetEventMesg(OS_EVENT_PRENMI, &WORLD(gIntrMesgQueue), (OSMesg) MESG_NMI_REQUEST);
}

void alloc_pool(void) {
    void *start = (void *) SEG_POOL_START;
    void *end = (void *) SEG_POOL_END;

    main_pool_init(start, end);
    WORLD(gEffectsMemoryPool) = mem_pool_init(0x4000, MEMORY_POOL_LEFT);
}

void create_thread(OSThread *thread, OSId id, void (*entry)(void *), void *arg, void *sp, OSPri pri) {
    thread->next = NULL;
    thread->queue = NULL;
    osCreateThread(thread, id, entry, arg, sp, pri);
}

#if defined(VERSION_SH) || defined(VERSION_CN)
extern void func_sh_802f69cc(void);
#endif

void handle_nmi_request(void) {
    WORLD(gResetTimer) = 1;
    WORLD(gNmiResetBarsTimer) = 0;
    stop_sounds_in_continuous_banks();
    sound_banks_disable(SEQ_PLAYER_SFX, SOUND_BANKS_BACKGROUND);
    fadeout_music(90);
#if defined(VERSION_SH) || defined(VERSION_CN)
    func_sh_802f69cc();
#endif
}

void receive_new_tasks(void) {
    struct SPTask *spTask;

    while (osRecvMesg(&WORLD(gSPTaskMesgQueue), (OSMesg *) &spTask, OS_MESG_NOBLOCK) != -1) {
        spTask->state = SPTASK_STATE_NOT_STARTED;
        switch (spTask->task.t.type) {
            case 2:
                WORLD(sNextAudioSPTask) = spTask;
                break;
            case 1:
                WORLD(sNextDisplaySPTask) = spTask;
                break;
        }
    }

    if (WORLD(sCurrentAudioSPTask) == NULL && WORLD(sNextAudioSPTask) != NULL) {
        WORLD(sCurrentAudioSPTask) = WORLD(sNextAudioSPTask);
        WORLD(sNextAudioSPTask) = NULL;
    }

    if (WORLD(sCurrentDisplaySPTask) == NULL && WORLD(sNextDisplaySPTask) != NULL) {
        WORLD(sCurrentDisplaySPTask) = WORLD(sNextDisplaySPTask);
        WORLD(sNextDisplaySPTask) = NULL;
    }
}

void start_sptask(s32 taskType) {
    UNUSED u8 filler[4];

    if (taskType == M_AUDTASK) {
        WORLD(gActiveSPTask) = WORLD(sCurrentAudioSPTask);
    } else {
        WORLD(gActiveSPTask) = WORLD(sCurrentDisplaySPTask);
    }

    osSpTaskLoad(&WORLD(gActiveSPTask)->task);
    osSpTaskStartGo(&WORLD(gActiveSPTask)->task);
    WORLD(gActiveSPTask)->state = SPTASK_STATE_RUNNING;
}

void interrupt_gfx_sptask(void) {
    if (WORLD(gActiveSPTask)->task.t.type == M_GFXTASK) {
        WORLD(gActiveSPTask)->state = SPTASK_STATE_INTERRUPTED;
        osSpTaskYield();
    }
}

void start_gfx_sptask(void) {
    if (WORLD(gActiveSPTask) == NULL && WORLD(sCurrentDisplaySPTask) != NULL
        && WORLD(sCurrentDisplaySPTask)->state == SPTASK_STATE_NOT_STARTED) {
        profiler_log_gfx_time(TASKS_QUEUED);
        start_sptask(M_GFXTASK);
    }
}

void pretend_audio_sptask_done(void) {
    WORLD(gActiveSPTask) = WORLD(sCurrentAudioSPTask);
    WORLD(gActiveSPTask)->state = SPTASK_STATE_RUNNING;
    osSendMesg(&WORLD(gIntrMesgQueue), (OSMesg) MESG_SP_COMPLETE, OS_MESG_NOBLOCK);
}

void handle_vblank(void) {
    UNUSED u8 filler[4];

    stub_main_3();
    WORLD(gNumVblanks)++;
#if defined(VERSION_SH) || defined(VERSION_CN)
    if (gResetTimer > 0 && gResetTimer < 100) {
        gResetTimer++;
    }
#else
    if (WORLD(gResetTimer) > 0) {
        WORLD(gResetTimer)++;
    }
#endif

    receive_new_tasks();

    // First try to kick off an audio task. If the gfx task is currently
    // running, we need to asynchronously interrupt it -- handle_sp_complete
    // will pick up on what we're doing and start the audio task for us.
    // If there is already an audio task running, there is nothing to do.
    // If there is no audio task available, try a gfx task instead.
    if (WORLD(sCurrentAudioSPTask) != NULL) {
        if (WORLD(gActiveSPTask) != NULL) {
            interrupt_gfx_sptask();
        } else {
            profiler_log_vblank_time();
            if (WORLD(sAudioEnabled)) {
                start_sptask(M_AUDTASK);
            } else {
                pretend_audio_sptask_done();
            }
        }
    } else {
        if (WORLD(gActiveSPTask) == NULL && WORLD(sCurrentDisplaySPTask) != NULL
            && WORLD(sCurrentDisplaySPTask)->state != SPTASK_STATE_FINISHED) {
            profiler_log_gfx_time(TASKS_QUEUED);
            start_sptask(M_GFXTASK);
        }
    }
#if ENABLE_RUMBLE
    rumble_thread_update_vi();
#endif

    // Notify the game loop about the vblank.
    if (WORLD(gVblankHandler1) != NULL) {
        osSendMesg(WORLD(gVblankHandler1)->queue, WORLD(gVblankHandler1)->msg, OS_MESG_NOBLOCK);
    }
    if (WORLD(gVblankHandler2) != NULL) {
        osSendMesg(WORLD(gVblankHandler2)->queue, WORLD(gVblankHandler2)->msg, OS_MESG_NOBLOCK);
    }
}

void handle_sp_complete(void) {
    struct SPTask *curSPTask = WORLD(gActiveSPTask);

    WORLD(gActiveSPTask) = NULL;

    if (curSPTask->state == SPTASK_STATE_INTERRUPTED) {
        // handle_vblank tried to start an audio task while there was already a
        // gfx task running, so it had to interrupt the gfx task. That interruption
        // just finished.
        if (osSpTaskYielded(&curSPTask->task) == 0) {
            // The gfx task completed before we had time to interrupt it.
            // Mark it finished, just like below.
            curSPTask->state = SPTASK_STATE_FINISHED;
            profiler_log_gfx_time(RSP_COMPLETE);
        }

        // Start the audio task, as expected by handle_vblank.
        profiler_log_vblank_time();
        if (WORLD(sAudioEnabled)) {
            start_sptask(M_AUDTASK);
        } else {
            pretend_audio_sptask_done();
        }
    } else {
        curSPTask->state = SPTASK_STATE_FINISHED;
        if (curSPTask->task.t.type == M_AUDTASK) {
            // After audio tasks come gfx tasks.
            profiler_log_vblank_time();
            if (WORLD(sCurrentDisplaySPTask) != NULL
                && WORLD(sCurrentDisplaySPTask)->state != SPTASK_STATE_FINISHED) {
                if (WORLD(sCurrentDisplaySPTask)->state != SPTASK_STATE_INTERRUPTED) {
                    profiler_log_gfx_time(TASKS_QUEUED);
                }
                start_sptask(M_GFXTASK);
            }
            WORLD(sCurrentAudioSPTask) = NULL;
            if (curSPTask->msgqueue != NULL) {
                osSendMesg(curSPTask->msgqueue, curSPTask->msg, OS_MESG_NOBLOCK);
            }
        } else {
            // The SP process is done, but there is still a Display Processor notification
            // that needs to arrive before we can consider the task completely finished and
            // null out sCurrentDisplaySPTask. That happens in handle_dp_complete.
            profiler_log_gfx_time(RSP_COMPLETE);
        }
    }
}

void handle_dp_complete(void) {
    // Gfx SP task is completely done.
    if (WORLD(sCurrentDisplaySPTask)->msgqueue != NULL) {
        osSendMesg(WORLD(sCurrentDisplaySPTask)->msgqueue, WORLD(sCurrentDisplaySPTask)->msg, OS_MESG_NOBLOCK);
    }
    profiler_log_gfx_time(RDP_COMPLETE);
    WORLD(sCurrentDisplaySPTask)->state = SPTASK_STATE_FINISHED_DP;
    WORLD(sCurrentDisplaySPTask) = NULL;
}

void thread3_main(UNUSED void *arg) {
    setup_mesg_queues();
    alloc_pool();
    load_engine_code_segment();

    create_thread(&WORLD(gSoundThread), 4, thread4_sound, NULL, WORLD(gThread4Stack) + 0x2000, 20);
    osStartThread(&WORLD(gSoundThread));

    create_thread(&WORLD(gGameLoopThread), 5, thread5_game_loop, NULL, WORLD(gThread5Stack) + 0x2000, 10);
    osStartThread(&WORLD(gGameLoopThread));

    while (TRUE) {
        OSMesg msg;

        osRecvMesg(&WORLD(gIntrMesgQueue), &msg, OS_MESG_BLOCK);
        switch ((uintptr_t) msg) {
            case MESG_VI_VBLANK:
                handle_vblank();
                break;
            case MESG_SP_COMPLETE:
                handle_sp_complete();
                break;
            case MESG_DP_COMPLETE:
                handle_dp_complete();
                break;
            case MESG_START_GFX_SPTASK:
                start_gfx_sptask();
                break;
            case MESG_NMI_REQUEST:
                handle_nmi_request();
                break;
        }
        stub_main_2();
    }
}

void set_vblank_handler(s32 index, struct VblankHandler *handler, OSMesgQueue *queue, OSMesg *msg) {
    handler->queue = queue;
    handler->msg = msg;

    switch (index) {
        case 1:
            WORLD(gVblankHandler1) = handler;
            break;
        case 2:
            WORLD(gVblankHandler2) = handler;
            break;
    }
}

void send_sp_task_message(OSMesg *msg) {
    osWritebackDCacheAll();
    osSendMesg(&WORLD(gSPTaskMesgQueue), msg, OS_MESG_NOBLOCK);
}

void dispatch_audio_sptask(struct SPTask *spTask) {
    if (WORLD(sAudioEnabled) && spTask != NULL) {
        osWritebackDCacheAll();
        osSendMesg(&WORLD(gSPTaskMesgQueue), spTask, OS_MESG_NOBLOCK);
    }
}

void exec_display_list(struct SPTask *spTask) {
    if (spTask != NULL) {
        // Library: what the RSP gets, for drawing (platform/draw.h).
        if (SM64_DRAW) {
            gHostDrawnList = spTask->task.t.data_ptr;
        }
        osWritebackDCacheAll();
        spTask->state = SPTASK_STATE_NOT_STARTED;
        if (WORLD(sCurrentDisplaySPTask) == NULL) {
            WORLD(sCurrentDisplaySPTask) = spTask;
            WORLD(sNextDisplaySPTask) = NULL;
            osSendMesg(&WORLD(gIntrMesgQueue), (OSMesg) MESG_START_GFX_SPTASK, OS_MESG_NOBLOCK);
        } else {
            WORLD(sNextDisplaySPTask) = spTask;
        }
    }
}

void turn_on_audio(void) {
    WORLD(sAudioEnabled) = TRUE;
}

void turn_off_audio(void) {
    WORLD(sAudioEnabled) = FALSE;
    while (WORLD(sCurrentAudioSPTask) != NULL) {
        ;
    }
}

/**
 * Initialize hardware, start main thread, then idle.
 */
void thread1_idle(UNUSED void *arg) {
#if defined(VERSION_US) || defined(VERSION_SH) || defined(VERSION_CN)
    s32 sp24 = WORLD(osTvType);
#endif

    osCreateViManager(OS_PRIORITY_VIMGR);
#if defined(VERSION_US) || defined(VERSION_SH) || defined(VERSION_CN)
    if (sp24 == TV_TYPE_NTSC) {
        osViSetMode(&WORLD(osViModeTable)[OS_VI_NTSC_LAN1]);
    } else {
        osViSetMode(&WORLD(osViModeTable)[OS_VI_PAL_LAN1]);
    }
#elif defined(VERSION_JP)
    osViSetMode(&WORLD(osViModeTable)[OS_VI_NTSC_LAN1]);
#else // VERSION_EU
    osViSetMode(&osViModeTable[OS_VI_PAL_LAN1]);
#endif
    osViBlack(TRUE);
    osViSetSpecialFeatures(OS_VI_DITHER_FILTER_ON);
    osViSetSpecialFeatures(OS_VI_GAMMA_OFF);
    osCreatePiManager(OS_PRIORITY_PIMGR, &WORLD(gPIMesgQueue), WORLD(gPIMesgBuf), ARRAY_COUNT(WORLD(gPIMesgBuf)));
    create_thread(&WORLD(gMainThread), 3, thread3_main, NULL, WORLD(gThread3Stack) + 0x2000, 100);
    if (WORLD(D_8032C650) == 0) {
        osStartThread(&WORLD(gMainThread));
    }
    osSetThreadPri(NULL, 0);

    // halt
    while (TRUE) {
        ;
    }
}

void main_func(void) {
    UNUSED u8 filler[64];

    osInitialize();
    stub_main_1();
    create_thread(&WORLD(gIdleThread), 1, thread1_idle, NULL, WORLD(gIdleThreadStack) + 0x800, 100);
    osStartThread(&WORLD(gIdleThread));
}

// Library: its variables' addresses (tools/state/types.py).
#include "pointers/game/src/game/main.c.inc.c"
