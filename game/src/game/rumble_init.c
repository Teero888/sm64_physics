#include "config.h"

#if ENABLE_RUMBLE

#include <ultra64.h>
// Library: the Rumble Pak's functions only; the whole of PR/os.h declares
// libultra with 32-bit addresses.
#include <PR/os_motor.h>
#include "macros.h"

#include "buffers/buffers.h"
#include "main.h"
#include "rumble_init.h"

FORCE_BSS OSThread gRumblePakThread;

FORCE_BSS OSPfs gRumblePakPfs;

FORCE_BSS OSMesg gRumblePakSchedulerMesgBuf;
FORCE_BSS OSMesgQueue gRumblePakSchedulerMesgQueue;
FORCE_BSS OSMesg gRumbleThreadVIMesgBuf;
FORCE_BSS OSMesgQueue gRumbleThreadVIMesgQueue;

FORCE_BSS struct RumbleData gRumbleDataQueue[3];
FORCE_BSS struct StructSH8031D9B0 gCurrRumbleSettings;

s32 sRumblePakThreadActive = FALSE;
s32 sRumblePakActive = FALSE;
s32 sRumblePakErrorCount = 0;
s32 gRumblePakTimer = 0;

void init_rumble_pak_scheduler_queue(void) {
    osCreateMesgQueue(&WORLD(gRumblePakSchedulerMesgQueue), &WORLD(gRumblePakSchedulerMesgBuf), 1);
    osSendMesg(&WORLD(gRumblePakSchedulerMesgQueue), (OSMesg) 0, OS_MESG_NOBLOCK);
}

void block_until_rumble_pak_free(void) {
    OSMesg msg;
    osRecvMesg(&WORLD(gRumblePakSchedulerMesgQueue), &msg, OS_MESG_BLOCK);
}

void release_rumble_pak_control(void) {
    osSendMesg(&WORLD(gRumblePakSchedulerMesgQueue), (OSMesg) 0, OS_MESG_NOBLOCK);
}

static void start_rumble(void) {
    if (!WORLD(sRumblePakActive)) {
        return;
    }

    block_until_rumble_pak_free();

#ifdef VERSION_CN
    if (!__osMotorAccess(&gRumblePakPfs, MOTOR_START)) {
#else
    if (!osMotorStart(&WORLD(gRumblePakPfs))) {
#endif
        WORLD(sRumblePakErrorCount) = 0;
    } else {
        WORLD(sRumblePakErrorCount)++;
    }

    release_rumble_pak_control();
}

static void stop_rumble(void) {
    if (!WORLD(sRumblePakActive)) {
        return;
    }

    block_until_rumble_pak_free();

#ifdef VERSION_CN
    if (!__osMotorAccess(&gRumblePakPfs, MOTOR_STOP)) {
#else
    if (!osMotorStop(&WORLD(gRumblePakPfs))) {
#endif
        WORLD(sRumblePakErrorCount) = 0;
    } else {
        WORLD(sRumblePakErrorCount)++;
    }

    release_rumble_pak_control();
}

static void update_rumble_pak(void) {
    if (WORLD(gResetTimer) > 0) {
        stop_rumble();
        return;
    }

    if (WORLD(gCurrRumbleSettings).unk08 > 0) {
        WORLD(gCurrRumbleSettings).unk08--;
        start_rumble();
    } else if (WORLD(gCurrRumbleSettings).unk04 > 0) {
        WORLD(gCurrRumbleSettings).unk04--;

        WORLD(gCurrRumbleSettings).unk02 -= WORLD(gCurrRumbleSettings).unk0E;
        if (WORLD(gCurrRumbleSettings).unk02 < 0) {
            WORLD(gCurrRumbleSettings).unk02 = 0;
        }

        if (WORLD(gCurrRumbleSettings).unk00 == 1) {
            start_rumble();
        } else if (WORLD(gCurrRumbleSettings).unk06 >= 0x100) {
            WORLD(gCurrRumbleSettings).unk06 -= 0x100;
            start_rumble();
        } else {
            WORLD(gCurrRumbleSettings).unk06 +=
                ((WORLD(gCurrRumbleSettings).unk02 * WORLD(gCurrRumbleSettings).unk02 * WORLD(gCurrRumbleSettings).unk02) / (1 << 9)) + 4;

            stop_rumble();
        }
    } else {
        WORLD(gCurrRumbleSettings).unk04 = 0;

        if (WORLD(gCurrRumbleSettings).unk0A >= 5) {
            start_rumble();
        } else if ((WORLD(gCurrRumbleSettings).unk0A >= 2) && (WORLD(gNumVblanks) % WORLD(gCurrRumbleSettings).unk0C == 0)) {
            start_rumble();
        } else {
            stop_rumble();
        }
    }

    if (WORLD(gCurrRumbleSettings).unk0A > 0) {
        WORLD(gCurrRumbleSettings).unk0A--;
    }
}

static void update_rumble_data_queue(void) {
    if (WORLD(gRumbleDataQueue)[0].unk00) {
        WORLD(gCurrRumbleSettings).unk06 = 0;
        WORLD(gCurrRumbleSettings).unk08 = 4;
        WORLD(gCurrRumbleSettings).unk00 = WORLD(gRumbleDataQueue)[0].unk00;
        WORLD(gCurrRumbleSettings).unk04 = WORLD(gRumbleDataQueue)[0].unk02;
        WORLD(gCurrRumbleSettings).unk02 = WORLD(gRumbleDataQueue)[0].unk01;
        WORLD(gCurrRumbleSettings).unk0E = WORLD(gRumbleDataQueue)[0].unk04;
    }

    WORLD(gRumbleDataQueue)[0] = WORLD(gRumbleDataQueue)[1];
    WORLD(gRumbleDataQueue)[1] = WORLD(gRumbleDataQueue)[2];

    WORLD(gRumbleDataQueue)[2].unk00 = 0;
}

void queue_rumble_data(s16 a0, s16 a1) {
    if (WORLD(gCurrDemoInput) != NULL) {
        return;
    }

    if (a1 > 70) {
        WORLD(gRumbleDataQueue)[2].unk00 = 1;
    } else {
        WORLD(gRumbleDataQueue)[2].unk00 = 2;
    }

    WORLD(gRumbleDataQueue)[2].unk01 = a1;
    WORLD(gRumbleDataQueue)[2].unk02 = a0;
    WORLD(gRumbleDataQueue)[2].unk04 = 0;
}

void func_sh_8024C89C(s16 a0) {
    WORLD(gRumbleDataQueue)[2].unk04 = a0;
}

u8 is_rumble_finished_and_queue_empty(void) {
    if (WORLD(gCurrRumbleSettings).unk08 + WORLD(gCurrRumbleSettings).unk04 >= 4) {
        return FALSE;
    }

    if (WORLD(gRumbleDataQueue)[0].unk00 != 0) {
        return FALSE;
    }

    if (WORLD(gRumbleDataQueue)[1].unk00 != 0) {
        return FALSE;
    }

    if (WORLD(gRumbleDataQueue)[2].unk00 != 0) {
        return FALSE;
    }

    return TRUE;
}

void reset_rumble_timers(void) {
    if (WORLD(gCurrDemoInput) != NULL) {
        return;
    }

    if (WORLD(gCurrRumbleSettings).unk0A == 0) {
        WORLD(gCurrRumbleSettings).unk0A = 7;
    }

    if (WORLD(gCurrRumbleSettings).unk0A < 4) {
        WORLD(gCurrRumbleSettings).unk0A = 4;
    }

    WORLD(gCurrRumbleSettings).unk0C = 7;
}

void reset_rumble_timers_2(s32 a0) {
    if (WORLD(gCurrDemoInput) != NULL) {
        return;
    }

    if (WORLD(gCurrRumbleSettings).unk0A == 0) {
        WORLD(gCurrRumbleSettings).unk0A = 7;
    }

    if (WORLD(gCurrRumbleSettings).unk0A < 4) {
        WORLD(gCurrRumbleSettings).unk0A = 4;
    }

    if (a0 == 4) {
        WORLD(gCurrRumbleSettings).unk0C = 1;
    }

    if (a0 == 3) {
        WORLD(gCurrRumbleSettings).unk0C = 2;
    }

    if (a0 == 2) {
        WORLD(gCurrRumbleSettings).unk0C = 3;
    }

    if (a0 == 1) {
        WORLD(gCurrRumbleSettings).unk0C = 4;
    }

    if (a0 == 0) {
        WORLD(gCurrRumbleSettings).unk0C = 5;
    }
}

void func_sh_8024CA04(void) {
    if (WORLD(gCurrDemoInput) != NULL) {
        return;
    }

    WORLD(gCurrRumbleSettings).unk0A = 4;
    WORLD(gCurrRumbleSettings).unk0C = 4;
}

// Library: the rumble thread's start and one iteration of its loop, which the
// host runs at the thread's start and at every vertical interrupt (it runs no
// threads; platform/host.c).
void rumble_thread_start(void) {
    CN_DEBUG_PRINTF(("start motor thread\n"));

    cancel_rumble();
    WORLD(sRumblePakThreadActive) = TRUE;

    CN_DEBUG_PRINTF(("go motor thread\n"));
}

void rumble_thread_vi(void) {
    if (!WORLD(sRumblePakThreadActive)) {
        return;
    }
    update_rumble_data_queue();
    update_rumble_pak();

    if (WORLD(sRumblePakActive)) {
        if (WORLD(sRumblePakErrorCount) >= 30) {
            WORLD(sRumblePakActive) = FALSE;
        }
    } else if (WORLD(gNumVblanks) % 60 == 0) {
        WORLD(sRumblePakActive) = osMotorInit(&WORLD(gSIEventMesgQueue), &WORLD(gRumblePakPfs), WORLD(gPlayer1Controller)->port) == 0;
        WORLD(sRumblePakErrorCount) = 0;
    }

    if (WORLD(gRumblePakTimer) > 0) {
        WORLD(gRumblePakTimer)--;
    }
}

static void thread6_rumble_loop(UNUSED void *a0) {
    OSMesg msg;

    rumble_thread_start();

    while (TRUE) {
        // Block until VI
        osRecvMesg(&WORLD(gRumbleThreadVIMesgQueue), &msg, OS_MESG_BLOCK);
        rumble_thread_vi();
    }
}

void cancel_rumble(void) {
    WORLD(sRumblePakActive) = osMotorInit(&WORLD(gSIEventMesgQueue), &WORLD(gRumblePakPfs), WORLD(gPlayer1Controller)->port) == 0;

    if (WORLD(sRumblePakActive)) {
#ifdef VERSION_CN
        __osMotorAccess(&gRumblePakPfs, MOTOR_STOP);
#else
        osMotorStop(&WORLD(gRumblePakPfs));
#endif
    }

    WORLD(gRumbleDataQueue)[0].unk00 = 0;
    WORLD(gRumbleDataQueue)[1].unk00 = 0;
    WORLD(gRumbleDataQueue)[2].unk00 = 0;

    WORLD(gCurrRumbleSettings).unk04 = 0;
    WORLD(gCurrRumbleSettings).unk0A = 0;

    WORLD(gRumblePakTimer) = 0;
}

void create_thread_6(void) {
    osCreateMesgQueue(&WORLD(gRumbleThreadVIMesgQueue), &WORLD(gRumbleThreadVIMesgBuf), 1);
    osCreateThread(&WORLD(gRumblePakThread), 6, thread6_rumble_loop, NULL, WORLD(gThread6Stack) + 0x2000, 30);
    osStartThread(&WORLD(gRumblePakThread));
}

void rumble_thread_update_vi(void) {
    if (!WORLD(sRumblePakThreadActive)) {
        return;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmultichar"
    osSendMesg(&WORLD(gRumbleThreadVIMesgQueue), (OSMesg) 'VRTC', OS_MESG_NOBLOCK);
#pragma GCC diagnostic pop
}

#endif

// Library: its variables' addresses (tools/state/types.py).
#include "pointers/game/src/game/rumble_init.c.inc.c"
