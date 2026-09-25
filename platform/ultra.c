// Stand-ins for the parts of libultra (the N64 SDK) the game calls. There are
// no threads, interrupts or coprocessors: whatever the game would wait for has
// already happened, DMA is a copy from data linked into the library, and the
// controller reads the input of the frame being stepped.
#include <ultra64.h>
#include <PR/os_pi.h>
#ifdef VERSION_SH
#include <PR/os_motor.h>
#endif
#include <macros.h>
#include <string.h>

#include "rsp_audio.h"

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
// An NTSC console, as the JP and US movies are recorded on.
u32 osTvType = 1;
static OSTime sTime;
// Counts calls, so anything that reads it is deterministic.
OSTime osGetTime(void) {
    return WORLD(sTime)++;
}
void osSetTime(OSTime time) {
    WORLD(sTime) = time;
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

#if defined(VERSION_SH)
// The Shindou Edition's audio reads the ROM through a PI handle; the "ROM"
// is the host's memory, as for osPiStartDma.
static OSPiHandle sCartHandle;

OSPiHandle *osCartRomInit(void) {
    return &WORLD(sCartHandle);
}

OSPiHandle *osDriveRomInit(void) {
    return &WORLD(sCartHandle);
}

s32 osEPiStartDma(UNUSED OSPiHandle *handle, OSIoMesg *mb, s32 direction) {
    return osPiStartDma(mb, mb->hdr.pri, direction, mb->devAddr, mb->dramAddr, mb->size, mb->hdr.retQueue);
}
#endif

#if defined(VERSION_SH)
// The CPU's cycle counter, which only seeds the Shindou Edition's sound
// randomness (gAudioRandom): the host has no cycles to count.
u32 osGetCount(void) {
    return 0;
}

// No Rumble Pak, as in TAS movies: osMotorInit finds none.
s32 osMotorInit(UNUSED OSMesgQueue *mq, UNUSED OSPfs *pfs, UNUSED int channel) {
    return PFS_ERR_NOPACK;
}

s32 osMotorStart(UNUSED OSPfs *pfs) {
    return PFS_ERR_NOPACK;
}

s32 osMotorStop(UNUSED OSPfs *pfs) {
    return PFS_ERR_NOPACK;
}
#endif

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
    pads[0] = WORLD(gHostPad);
    for (int i = 1; i < 4; ++i) {
        memset(&pads[i], 0, sizeof(pads[i]));
        pads[i].errnum = CONT_NO_RESPONSE_ERROR;
    }
}

// A 4 Kbit EEPROM as an emulator formats it: every byte 0xFF.
static u8 sEeprom[EEPROM_MAXBLOCKS * EEPROM_BLOCK_SIZE];
static int sEepromFormatted;

static void format_eeprom(void) {
    if (!WORLD(sEepromFormatted)) {
        memset(WORLD(sEeprom), 0xff, sizeof(WORLD(sEeprom)));
        WORLD(sEepromFormatted) = 1;
    }
}

s32 osEepromProbe(UNUSED OSMesgQueue *mq) {
    return EEPROM_TYPE_4K;
}

s32 osEepromLongRead(UNUSED OSMesgQueue *mq, u8 address, u8 *buffer, int nbytes) {
    format_eeprom();
    if (address * EEPROM_BLOCK_SIZE + nbytes > (int) sizeof(WORLD(sEeprom))) {
        return -1;
    }
    memcpy(buffer, WORLD(sEeprom) + address * EEPROM_BLOCK_SIZE, nbytes);
    return 0;
}

s32 osEepromLongWrite(UNUSED OSMesgQueue *mq, u8 address, u8 *buffer, int nbytes) {
    format_eeprom();
    if (address * EEPROM_BLOCK_SIZE + nbytes > (int) sizeof(WORLD(sEeprom))) {
        return -1;
    }
    memcpy(WORLD(sEeprom) + address * EEPROM_BLOCK_SIZE, buffer, nbytes);
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

// The audio interface: it plays the buffer the sound thread handed it and
// holds the next one (a third is refused), at the frequency its DAC divides
// the video clock to. The sound thread sizes its buffers by what is left of
// the one playing. With sound on, each vertical interrupt plays a sixtieth
// (PAL: a fiftieth) of a second of them (host_ai_vi); what the thread hands
// it during a step is the sound of that step (sm64_audio).
#ifdef VERSION_EU
#define VI_CLOCK 49656530 // osViClock, PAL
#define VI_RATE 50
#else
#define VI_CLOCK 48681812 // NTSC
#define VI_RATE 60
#endif

s32 gHostAiFrequency;
static u32 sAiSamples[2]; // stereo samples left of the buffer playing and of the next
static u32 sAiClock;      // what the last interrupts played beyond whole samples, in 1/VI_RATE
s16 gHostAudio[HOST_AUDIO_MAX * 2];
u32 gHostAudioSamples;

s32 osAiSetFrequency(u32 frequency) {
    const u32 dacRate = (u32) ((f32) VI_CLOCK / (f32) frequency + 0.5f);
    if (dacRate < 132) { // AI_MIN_DAC_RATE
        return -1;
    }
    WORLD(gHostAiFrequency) = VI_CLOCK / (s32) dacRate;
    return WORLD(gHostAiFrequency);
}

s32 osAiSetNextBuffer(void *buf, u32 size) {
    if (WORLD(sAiSamples)[1] != 0) {
        return -1;
    }
    const u32 samples = size / 4;
    WORLD(sAiSamples)[WORLD(sAiSamples)[0] != 0] = samples;
    const u32 room = HOST_AUDIO_MAX - WORLD(gHostAudioSamples);
    memcpy(WORLD(gHostAudio) + WORLD(gHostAudioSamples) * 2, buf, (samples < room ? samples : room) * 4);
    WORLD(gHostAudioSamples) += samples < room ? samples : room;
    return 0;
}

u32 osAiGetLength(void) {
    return WORLD(sAiSamples)[0] * 4;
}

void host_ai_vi(void) {
    WORLD(sAiClock) += (u32) WORLD(gHostAiFrequency);
    u32 played = WORLD(sAiClock) / VI_RATE;
    WORLD(sAiClock) %= VI_RATE;
    while (played != 0 && WORLD(sAiSamples)[0] != 0) {
        const u32 n = played < WORLD(sAiSamples)[0] ? played : WORLD(sAiSamples)[0];
        WORLD(sAiSamples)[0] -= n;
        played -= n;
        if (WORLD(sAiSamples)[0] == 0) {
            WORLD(sAiSamples)[0] = WORLD(sAiSamples)[1];
            WORLD(sAiSamples)[1] = 0;
        }
    }
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

// Library: its variables' addresses (tools/state/types.py).
#include "pointers/platform/ultra.c.inc.c"
