// The RSP's audio microcode (platform/rsp_audio.c).
#pragma once
#include <stddef.h>
#include <ultra64.h>
#include <PR/abi.h>

// Its tables, from a normalized ROM.
void rsp_audio_load(const unsigned char *rom, size_t size);
// Runs a sound thread's command list (an M_AUDTASK's data) at once.
void rsp_audio_run(const Acmd *commands, size_t count);

// The audio interface (platform/ultra.c), in the current world: its
// frequency, one vertical interrupt of it playing, and the sound handed to it
// since the step began (stereo samples, left first).
#define HOST_AUDIO_MAX 32768
extern s32 gHostAiFrequency;
void host_ai_vi(void);
extern s16 gHostAudio[HOST_AUDIO_MAX * 2];
extern u32 gHostAudioSamples;
