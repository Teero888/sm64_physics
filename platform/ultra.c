// Stand-ins for the parts of libultra (the N64 SDK) the game calls. There are
// no threads, interrupts or coprocessors: whatever the game would wait for has
// already happened, DMA is a copy from data linked into the library, and the
// controller reads the input of the frame being stepped.
#include <ultra64.h>
#include <PR/os_pi.h>
#include <macros.h>
#include <string.h>

// --- Messages ----------------------------------------------------------------

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count) {
    mq->mtqueue = NULL;
    mq->fullqueue = NULL;
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = count;
    mq->msg = msg;
}

s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, UNUSED s32 flag) {
    if (mq->validCount >= mq->msgCount) {
        return -1;
    }
    mq->msg[(mq->first + mq->validCount) % mq->msgCount] = msg;
    mq->validCount++;
    return 0;
}

// Blocking on an empty queue returns at once: the vertical interrupt, the
// finished frame or the controller read it waits for has already happened.
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag) {
    if (mq->validCount == 0) {
        if (flag == OS_MESG_NOBLOCK) {
            return -1;
        }
        if (msg != NULL) {
            *msg = NULL;
        }
        return 0;
    }
    if (msg != NULL) {
        *msg = mq->msg[mq->first];
    }
    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;
    return 0;
}

void osSetEventMesg(UNUSED OSEvent event, UNUSED OSMesgQueue *mq, UNUSED OSMesg msg) {
}

// --- Threads, memory, time ---------------------------------------------------

void osInitialize(void) {
}
void osCreateThread(UNUSED OSThread *t, UNUSED OSId id, UNUSED void (*entry)(void *), UNUSED void *arg,
                    UNUSED void *sp, UNUSED OSPri pri) {
}
void osStartThread(UNUSED OSThread *t) {
}
void osSetThreadPri(UNUSED OSThread *t, UNUSED OSPri pri) {
}
void osMapTLB(UNUSED s32 index, UNUSED OSPageMask mask, UNUSED void *vaddr, UNUSED u32 even, UNUSED u32 odd,
              UNUSED s32 asid) {
}
void osUnmapTLBAll(void) {
}
void osInvalDCache(UNUSED void *vaddr, UNUSED size_t size) {
}
void osWritebackDCache(UNUSED void *vaddr, UNUSED size_t size) {
}
void osWritebackDCacheAll(void) {
}
uintptr_t osVirtualToPhysical(void *addr) {
    return (uintptr_t) addr;
}

u64 osClockRate = 62500000;
static OSTime sTime;
// Counts calls, so anything that reads it is deterministic.
OSTime osGetTime(void) {
    return sTime++;
}
void osSetTime(OSTime time) {
    sTime = time;
}

// --- Cartridge DMA -------------------------------------------------------------

void osCreatePiManager(UNUSED OSPri pri, UNUSED OSMesgQueue *cmdQ, UNUSED OSMesg *cmdBuf, UNUSED s32 count) {
}

// "ROM" addresses are the addresses of the data linked into the library.
s32 osPiStartDma(OSIoMesg *mb, UNUSED s32 priority, s32 direction, uintptr_t devAddr, void *vAddr, size_t nbytes,
                 OSMesgQueue *mq) {
    if (direction == OS_READ) {
        memcpy(vAddr, (const void *) devAddr, nbytes);
    }
    if (mq != NULL) {
        osSendMesg(mq, (OSMesg) mb, OS_MESG_NOBLOCK);
    }
    return 0;
}

// --- Controllers and EEPROM ----------------------------------------------------------

extern OSContPad gHostPad;

// One standard controller in port 1, no pak: how TAS movies are recorded.
s32 osContInit(UNUSED OSMesgQueue *mq, u8 *bitpattern, OSContStatus *status) {
    for (int i = 0; i < 4; ++i) {
        status[i].type = i == 0 ? CONT_TYPE_NORMAL : 0;
        status[i].status = 0;
        status[i].errnum = i == 0 ? 0 : CONT_NO_RESPONSE_ERROR;
    }
    *bitpattern = 1;
    return 0;
}

s32 osContStartReadData(UNUSED OSMesgQueue *mq) {
    return 0;
}

void osContGetReadData(OSContPad *pads) {
    pads[0] = gHostPad;
    for (int i = 1; i < 4; ++i) {
        memset(&pads[i], 0, sizeof(pads[i]));
        pads[i].errnum = CONT_NO_RESPONSE_ERROR;
    }
}

// A 4 Kbit EEPROM as an emulator formats it: every byte 0xFF.
static u8 sEeprom[EEPROM_MAXBLOCKS * EEPROM_BLOCK_SIZE];
static int sEepromFormatted;

static void format_eeprom(void) {
    if (!sEepromFormatted) {
        memset(sEeprom, 0xff, sizeof(sEeprom));
        sEepromFormatted = 1;
    }
}

s32 osEepromProbe(UNUSED OSMesgQueue *mq) {
    return EEPROM_TYPE_4K;
}

s32 osEepromLongRead(UNUSED OSMesgQueue *mq, u8 address, u8 *buffer, int nbytes) {
    format_eeprom();
    if (address * EEPROM_BLOCK_SIZE + nbytes > (int) sizeof(sEeprom)) {
        return -1;
    }
    memcpy(buffer, sEeprom + address * EEPROM_BLOCK_SIZE, nbytes);
    return 0;
}

s32 osEepromLongWrite(UNUSED OSMesgQueue *mq, u8 address, u8 *buffer, int nbytes) {
    format_eeprom();
    if (address * EEPROM_BLOCK_SIZE + nbytes > (int) sizeof(sEeprom)) {
        return -1;
    }
    memcpy(sEeprom + address * EEPROM_BLOCK_SIZE, buffer, nbytes);
    return 0;
}

// --- Video, audio and the RSP: output only -------------------------------------------

OSViMode osViModeTable[42];
void osCreateViManager(UNUSED OSPri pri) {
}
void osViSetMode(UNUSED OSViMode *mode) {
}
void osViSetEvent(UNUSED OSMesgQueue *mq, UNUSED OSMesg msg, UNUSED u32 retraceCount) {
}
void osViSetSpecialFeatures(UNUSED u32 func) {
}
void osViSwapBuffer(UNUSED void *vaddr) {
}
void osViBlack(UNUSED u8 active) {
}

s32 osAiSetFrequency(u32 frequency) {
    return (s32) frequency;
}
s32 osAiSetNextBuffer(UNUSED void *buf, UNUSED u32 size) {
    return 0;
}
u32 osAiGetLength(void) {
    return 0;
}

void osSpTaskLoad(UNUSED OSTask *task) {
}
void osSpTaskStartGo(UNUSED OSTask *task) {
}
void osSpTaskYield(void) {
}
OSYieldResult osSpTaskYielded(UNUSED OSTask *task) {
    return 0;
}

// Microcode is never run; the game only takes the addresses.
u64 rspF3DBootStart[1], rspF3DBootEnd[1];
u64 rspF3DStart[1], rspF3DEnd[1];
u64 rspF3DDataStart[1], rspF3DDataEnd[1];
u64 rspAspMainStart[1], rspAspMainEnd[1];
u64 rspAspMainDataStart[1], rspAspMainDataEnd[1];
