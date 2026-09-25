// The RSP's audio microcode (aspMain), for the sound thread's command lists:
// what the commands do, from the SDK's abi.h and the decomp's rsp/audio.s.
// It works in a DMEM of its own and reads and writes the world's memory at
// the addresses the commands hold. Everything a command keeps from one task
// to the next (decoder, resampler and envelope states) it keeps there, so
// nothing here is state.
//
// DMEM holds bytes as they come from the world's memory; the samples in it
// and in the world's buffers are the host's int16_t.
#include <ultra64.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "rsp_audio.h"

// The resampler's filter: four taps for each of 64 fractional positions,
// from the microcode's data in the user's ROM.
static int16_t sResampleTable[64][4];

void rsp_audio_load(const unsigned char *rom, size_t size) {
    // Its first entries: they occur once in each version's ROM.
    static const unsigned char start[] = { 0x0c, 0x39, 0x66, 0xad, 0x0d, 0x46, 0xff, 0xdf,
                                           0x0b, 0x39, 0x66, 0x96, 0x0e, 0x5f, 0xff, 0xd8 };
    for (size_t at = 0; at + sizeof(sResampleTable) <= size; at += 2) {
        if (memcmp(rom + at, start, sizeof(start)) == 0) {
            for (int i = 0; i < 256; ++i) {
                sResampleTable[i / 4][i % 4] = (int16_t) (rom[at + i * 2] << 8 | rom[at + i * 2 + 1]);
            }
            return;
        }
    }
}

#define DMEM_SIZE 0x1000

struct rsp {
    uint8_t dmem[DMEM_SIZE + 16];
    // aSetBuffer
    uint16_t in, out, count;
    uint16_t dry_right, wet_left, wet_right;
    // aSetVolume
    int16_t vol[2], target[2];
    int32_t rate[2];
    int16_t dry, wet;
    // aLoadADPCM, aSetLoop
    int16_t book[8 * 16 * 2];
    int16_t *loop;
    // Shindou: aEnvSetup1 and aEnvSetup2 (volumes of two blocks of 8
    // samples, left, right and reverb, and their ramps), aFilter's first call
    uint16_t env[6];
    int16_t ramp[3];
    uint16_t filter_count;
    int16_t filter[8];
};

static int16_t *sample(struct rsp *r, uint32_t address) {
    return (int16_t *) (r->dmem + (address & (DMEM_SIZE - 2)));
}

static uint8_t *byte(struct rsp *r, uint32_t address) {
    return r->dmem + (address & (DMEM_SIZE - 1));
}

static int16_t clamp16(int32_t x) {
    return x < -0x8000 ? -0x8000 : x > 0x7fff ? 0x7fff : (int16_t) x;
}

static uint32_t align(uint32_t x, uint32_t to) {
    return (x + to - 1) & ~(to - 1);
}

// Between DMEM and the world's memory. DMEM wraps; the counts the game uses
// stay inside it.
static void load(struct rsp *r, uint16_t dmem, const void *from, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        *byte(r, dmem + i) = ((const uint8_t *) from)[i];
    }
}

static void save(struct rsp *r, void *to, uint16_t dmem, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        ((uint8_t *) to)[i] = *byte(r, dmem + i);
    }
}

// 9 bytes of input (a scale and predictor index, 16 4-bit residuals) into 16
// samples, from the two before them.
static void adpcm(struct rsp *r, bool init, bool loop, uint16_t out, uint16_t in, uint32_t count,
                  int16_t *state) {
    int16_t last[16];
    if (init) {
        memset(last, 0, sizeof(last));
    } else {
        memcpy(last, loop ? r->loop : state, sizeof(last));
    }
    for (int i = 0; i < 16; ++i, out += 2) {
        *sample(r, out) = last[i];
    }
    for (; count >= 32; count -= 32) {
        const uint8_t header = *byte(r, in++);
        const int shift = (header >> 4) < 12 ? 12 - (header >> 4) : 0;
        const int16_t *book = r->book + (header & 0xf) * 16;
        int16_t residual[16];
        for (int i = 0; i < 8; ++i) {
            const uint8_t b = *byte(r, in++);
            residual[i * 2] = (int16_t) ((b & 0xf0) << 8) >> shift;
            residual[i * 2 + 1] = (int16_t) ((b & 0x0f) << 12) >> shift;
        }
        // Each half of 8 predicts from the two samples before it.
        for (int half = 0; half < 2; ++half) {
            const int16_t *r8 = residual + half * 8;
            const int16_t l1 = half ? last[6] : last[14], l2 = half ? last[7] : last[15];
            int16_t *dst = last + half * 8;
            for (int i = 0; i < 8; ++i) {
                int32_t acc = (int32_t) r8[i] << 11;
                acc += book[i] * l1 + book[8 + i] * l2;
                for (int j = 0; j < i; ++j) {
                    acc += book[8 + j] * r8[i - 1 - j];
                }
                dst[i] = clamp16(acc >> 11);
            }
        }
        for (int i = 0; i < 16; ++i, out += 2) {
            *sample(r, out) = last[i];
        }
    }
    memcpy(state, last, sizeof(last));
}

// Resamples by pitch (UQ1.15) with the 4-tap filter; its state is the four
// source samples after where it stopped and the fractional position. Before
// the input go the state's samples, where the filter starts: all four, or
// (Shindou's flags) two of them, or each of them twice.
enum { PREFIX_4, PREFIX_2, PREFIX_8 };
static void resample(struct rsp *r, bool init, int prefix, uint16_t out, uint16_t in, uint32_t count,
                     uint32_t pitch, int16_t *state) {
    int16_t saved[4] = { 0 };
    uint32_t fraction = 0;
    if (!init) {
        memcpy(saved, state, sizeof(saved));
        fraction = (uint16_t) state[4];
    }
    uint32_t position = in >> 1;
    if (prefix == PREFIX_2) {
        position -= 2;
        *sample(r, position * 2) = saved[0];
        *sample(r, (position + 1) * 2) = saved[2];
    } else if (prefix == PREFIX_8) {
        position -= 8;
        for (int k = 0; k < 8; ++k) {
            *sample(r, (position + k) * 2) = saved[k / 2];
        }
    } else {
        position -= 4;
        for (int k = 0; k < 4; ++k) {
            *sample(r, (position + k) * 2) = saved[k];
        }
    }
    for (count >>= 1; count != 0; --count, out += 2) {
        const int16_t *taps = sResampleTable[(fraction & 0xfc00) >> 10];
        int32_t acc = 0;
        for (int k = 0; k < 4; ++k) {
            acc += *sample(r, (position + k) * 2) * taps[k];
        }
        *sample(r, out) = clamp16(acc >> 15);
        fraction += pitch;
        position += fraction >> 16;
        fraction &= 0xffff;
    }
    for (int k = 0; k < 4; ++k) {
        state[k] = *sample(r, (position + k) * 2);
    }
    state[4] = (int16_t) fraction;
}

// Signed 16-bit saturating addition, as the vector unit's vadd.
static int16_t add16(int16_t a, int16_t b) {
    return clamp16((int32_t) a + b);
}

static void dmem_move(struct rsp *r, uint16_t to, uint16_t from, uint32_t count) {
    // 16 bytes at a time, from the start.
    for (uint32_t i = 0; i < align(count, 16); ++i) {
        *byte(r, to + i) = *byte(r, from + i);
    }
}

static void clear(struct rsp *r, uint16_t dmem, uint32_t count) {
    for (uint32_t i = 0; i < align(count, 16); ++i) {
        *byte(r, dmem + i) = 0;
    }
}

#if !defined(VERSION_SH)

// A volume moving toward its target, in 16.16.
struct ramp {
    int32_t value, target, step;
};

static int16_t ramp_step(struct ramp *ramp) {
    ramp->value += ramp->step;
    if (ramp->step <= 0 ? ramp->value <= ramp->target : ramp->value >= ramp->target) {
        ramp->value = ramp->target;
        ramp->step = 0;
    }
    return (int16_t) (ramp->value >> 16);
}

static void mix(int16_t *dst, int16_t src, int16_t gain) {
    *dst = clamp16(*dst + ((src * gain) >> 15));
}

// What aEnvMixer keeps in the world's memory (ENVMIX_STATE, 80 bytes).
struct envmix_state {
    int16_t wet, dry;
    int32_t target[2], rate[2], sequence[2], value[2];
};
_Static_assert(sizeof(struct envmix_state) <= 80, "ENVMIX_STATE");

// Mixes a mono voice into the dry (and with A_AUX, the wet) channels, its
// left and right volumes moving along a ramp: every 8 samples the step is an
// eighth of the way to the next term of a geometric sequence.
static void envmixer(struct rsp *r, bool init, bool aux, struct envmix_state *state) {
    struct ramp ramps[2];
    int32_t sequence[2], rate[2];
    int16_t dry = r->dry, wet = r->wet;
    if (init) {
        for (int i = 0; i < 2; ++i) {
            ramps[i].value = (int32_t) r->vol[i] << 16;
            ramps[i].target = (int32_t) r->target[i] << 16;
            rate[i] = r->rate[i];
            sequence[i] = r->vol[i] * r->rate[i];
        }
    } else {
        wet = state->wet;
        dry = state->dry;
        for (int i = 0; i < 2; ++i) {
            ramps[i].target = state->target[i];
            rate[i] = state->rate[i];
            sequence[i] = state->sequence[i];
            ramps[i].value = state->value[i];
        }
    }
    for (int i = 0; i < 2; ++i) {
        ramps[i].step = ramps[i].target - ramps[i].value;
    }
    const uint16_t outputs[4] = { r->out, r->dry_right, r->wet_left, r->wet_right };
    uint32_t n = 0;
    for (uint32_t y = 0; y < r->count; y += 16) {
        for (int i = 0; i < 2; ++i) {
            if (ramps[i].step != 0) {
                sequence[i] = (int32_t) (((int64_t) sequence[i] * rate[i]) >> 16);
                ramps[i].step = (sequence[i] - ramps[i].value) >> 3;
            }
        }
        for (int x = 0; x < 8; ++x, ++n) {
            const int16_t left = ramp_step(&ramps[0]), right = ramp_step(&ramps[1]);
            const int16_t gains[4] = { clamp16((left * dry + 0x4000) >> 15), clamp16((right * dry + 0x4000) >> 15),
                                       clamp16((left * wet + 0x4000) >> 15), clamp16((right * wet + 0x4000) >> 15) };
            const int16_t in = *sample(r, r->in + n * 2);
            for (int c = 0; c < (aux ? 4 : 2); ++c) {
                mix(sample(r, outputs[c] + n * 2), in, gains[c]);
            }
        }
    }
    state->wet = wet;
    state->dry = dry;
    for (int i = 0; i < 2; ++i) {
        state->target[i] = ramps[i].target;
        state->rate[i] = rate[i];
        state->sequence[i] = sequence[i];
        state->value[i] = ramps[i].value;
    }
}

void rsp_audio_run(const Acmd *commands, size_t count) {
    // DMEM is the task's: what the microcode keeps, it keeps in the world's memory.
    struct rsp r;
    memset(&r, 0, sizeof(r));
    for (size_t c = 0; c < count; ++c) {
        const uint32_t w0 = (uint32_t) commands[c].words.w0;
        const uintptr_t w1 = commands[c].words.w1;
        const uint8_t flags = (uint8_t) (w0 >> 16);
        switch (w0 >> 24) {
            case A_SPNOOP:
            case A_SEGMENT: // addresses are the host's
                break;
            case A_ADPCM:
                adpcm(&r, flags & A_INIT, flags & A_LOOP, r.out, r.in, align(r.count, 32), (int16_t *) w1);
                break;
            case A_CLEARBUFF:
                clear(&r, (uint16_t) w0, (uint16_t) w1);
                break;
            case A_ENVMIXER:
                envmixer(&r, flags & A_INIT, flags & A_AUX, (struct envmix_state *) w1);
                break;
            case A_LOADBUFF:
                load(&r, r.in, (const void *) w1, align(r.count, 8));
                break;
            case A_RESAMPLE:
                resample(&r, flags & A_INIT, PREFIX_4, r.out, r.in, align(r.count, 16),
                         (uint32_t) (uint16_t) w0 << 1, (int16_t *) w1);
                break;
            case A_SAVEBUFF:
                save(&r, (void *) w1, r.out, align(r.count, 8));
                break;
            case A_SETBUFF:
                if (flags & A_AUX) {
                    r.dry_right = (uint16_t) w0;
                    r.wet_left = (uint16_t) (w1 >> 16);
                    r.wet_right = (uint16_t) w1;
                } else {
                    r.in = (uint16_t) w0;
                    r.out = (uint16_t) (w1 >> 16);
                    r.count = (uint16_t) w1;
                }
                break;
            case A_SETVOL:
                if (flags & A_AUX) {
                    r.dry = (int16_t) w0;
                    r.wet = (int16_t) w1;
                } else {
                    const int side = (flags & A_LEFT) ? 0 : 1;
                    if (flags & A_VOL) {
                        r.vol[side] = (int16_t) w0;
                    } else {
                        r.target[side] = (int16_t) w0;
                        r.rate[side] = (int32_t) (uint32_t) w1;
                    }
                }
                break;
            case A_DMEMMOVE:
                dmem_move(&r, (uint16_t) (w1 >> 16), (uint16_t) w0, (uint16_t) w1);
                break;
            case A_LOADADPCM: {
                const uint32_t n = align(w0 & 0xffffff, 8);
                memcpy(r.book, (const void *) w1, n < sizeof(r.book) ? n : sizeof(r.book));
                break;
            }
            case A_MIXER: {
                const int16_t gain = (int16_t) w0;
                const uint16_t in = (uint16_t) (w1 >> 16), out = (uint16_t) w1;
                for (uint32_t i = 0; i < align(r.count, 32); i += 2) {
                    mix(sample(&r, out + i), *sample(&r, in + i), gain);
                }
                break;
            }
            case A_INTERLEAVE: {
                // Into a copy first: the output may overlap the inputs.
                const uint16_t left = (uint16_t) (w1 >> 16), right = (uint16_t) w1;
                int16_t out[DMEM_SIZE / 2];
                const uint32_t n = r.count / 2;
                for (uint32_t i = 0; i < n && i < DMEM_SIZE / 4; ++i) {
                    out[i * 2] = *sample(&r, left + i * 2);
                    out[i * 2 + 1] = *sample(&r, right + i * 2);
                }
                for (uint32_t i = 0; i < n * 2 && i < DMEM_SIZE / 2; ++i) {
                    *sample(&r, r.out + i * 2) = out[i];
                }
                break;
            }
            case A_SETLOOP:
                r.loop = (int16_t *) w1;
                break;
            default: // A_POLEF: not used by the game
                break;
        }
    }
}

#else

// The Shindou Edition's microcode, a later one: commands of its own
// (abi.h), and the old ones with their counts in the command.

// Expands 8-bit samples; its state, as aADPCMdec's, is the last 16 samples.
static void s8dec(struct rsp *r, bool init, bool loop, uint16_t out, uint16_t in, uint32_t count,
                  int16_t *state) {
    int16_t last[16] = { 0 };
    if (!init) {
        memcpy(last, loop ? r->loop : state, sizeof(last));
    }
    for (int i = 0; i < 16; ++i) {
        *sample(r, out + i * 2) = last[i];
    }
    out += 32;
    for (int32_t n = (int32_t) count; n > 0; n -= 32, in += 16, out += 32) {
        for (int i = 0; i < 16; ++i) {
            *sample(r, out + i * 2) = (int16_t) (*byte(r, in + i) << 8);
        }
    }
    for (int i = 0; i < 16; ++i) {
        state[i] = *sample(r, out - 32 + i * 2);
    }
}

// Mixes a mono voice into dry left and right and wet left and right, 16
// samples at a time: two blocks of 8, each with its volumes (u16 fractions;
// a negated side is its one's complement), the wet send from the dry
// samples, swapped with swap. The game negates the side opposite a note
// panned hard to one side ("stereo strong"; JP/US/EU subtract it with aMix);
// mupen64plus's HLE of this microcode leaves it out, the microcode does not.
static void envmixer(struct rsp *r, uint32_t w0, uint32_t w1) {
    const uint16_t in = (w0 >> 12) & 0xff0;
    const uint16_t dl = (w1 >> 20) & 0xff0, dr = (w1 >> 12) & 0xff0, wl = (w1 >> 4) & 0xff0, wr = (w1 << 4) & 0xff0;
    const int16_t neg_left = (w0 & 2) ? -1 : 0, neg_right = (w0 & 1) ? -1 : 0;
    const bool swap = w0 & 4;
    uint16_t vol[6];
    memcpy(vol, r->env, sizeof(vol));
    int32_t n = (w0 >> 8) & 0xff;
    uint32_t at = 0;
    do {
        for (int block = 0; block < 2; ++block) {
            for (int i = 0; i < 8; ++i, at += 2) {
                const int16_t x = *sample(r, in + at);
                const int16_t left = (int16_t) (((x * (int32_t) vol[0 + block]) >> 16) ^ neg_left);
                const int16_t right = (int16_t) (((x * (int32_t) vol[2 + block]) >> 16) ^ neg_right);
                const int16_t wet_left = (int16_t) ((left * (int32_t) vol[4 + block]) >> 16);
                const int16_t wet_right = (int16_t) ((right * (int32_t) vol[4 + block]) >> 16);
                *sample(r, dl + at) = add16(*sample(r, dl + at), left);
                *sample(r, dr + at) = add16(*sample(r, dr + at), right);
                *sample(r, wl + at) = add16(*sample(r, wl + at), swap ? wet_right : wet_left);
                *sample(r, wr + at) = add16(*sample(r, wr + at), swap ? wet_left : wet_right);
            }
        }
        for (int i = 0; i < 6; ++i) {
            vol[i] = (uint16_t) (vol[i] + 2 * r->ramp[i / 2]);
        }
        n -= 16;
    } while (n > 0);
}

// An 8-tap filter over the samples at dmem in place: each output is the
// taps against it and the 7 inputs before it (the state holds the 8 before
// the first), rounded.
static void filter(struct rsp *r, bool zero_state, uint16_t dmem, int16_t *state) {
    int16_t y[16] = { 0 }; // the previous block, then the current one
    if (!zero_state) {
        memcpy(y, state, 16);
    }
    for (int32_t n = r->filter_count; n > 0; n -= 16, dmem += 16) {
        for (int i = 0; i < 8; ++i) {
            y[8 + i] = *sample(r, dmem + i * 2);
        }
        for (int j = 0; j < 8; ++j) {
            int64_t acc = 0;
            for (int k = 0; k < 8; ++k) {
                acc += (int32_t) r->filter[k] * y[8 + j - k];
            }
            *sample(r, dmem + j * 2) = clamp16((int32_t) ((acc * 2 + 0x8000) >> 16));
        }
        memcpy(y, y + 8, 16);
    }
    memcpy(state, y, 16);
}

void rsp_audio_run(const Acmd *commands, size_t count) {
    // DMEM is the task's: what the microcode keeps, it keeps in the world's memory.
    struct rsp r;
    memset(&r, 0, sizeof(r));
    for (size_t c = 0; c < count; ++c) {
        const uint32_t w0 = (uint32_t) commands[c].words.w0;
        const uintptr_t w1 = commands[c].words.w1;
        const uint8_t flags = (uint8_t) (w0 >> 16);
        switch (w0 >> 24) {
            case A_ADPCM:
                adpcm(&r, flags & A_INIT, flags & A_LOOP, r.out, r.in, align(r.count, 32), (int16_t *) w1);
                break;
            case A_CLEARBUFF:
                clear(&r, (uint16_t) w0, (uint16_t) w1);
                break;
            case A_ADDMIXER: {
                const uint16_t in = (uint16_t) (w1 >> 16), out = (uint16_t) w1;
                for (int32_t n = (w0 >> 12) & 0xff0, at = 0; n > 0; n -= 64) {
                    for (int i = 0; i < 32; ++i, at += 2) {
                        *sample(&r, out + at) = add16(*sample(&r, out + at), *sample(&r, in + at));
                    }
                }
                break;
            }
            case A_RESAMPLE:
                resample(&r, flags & A_INIT, (flags & 2) ? PREFIX_2 : (flags & 4) ? PREFIX_8 : PREFIX_4, r.out, r.in,
                         align(r.count, 16), (uint32_t) (uint16_t) w0 << 1, (int16_t *) w1);
                break;
            case A_RESAMPLE_ZOH: {
                // The nearest sample, at a position of 16.16 in bytes.
                const uint32_t step = (w0 & 0xffff) << 2;
                uint32_t position = (uint32_t) r.in << 16 | (w1 & 0xffff);
                uint16_t out = r.out;
                for (int32_t n = r.count; n > 0; n -= 8) {
                    for (int i = 0; i < 4; ++i, out += 2, position += step) {
                        *sample(&r, out) = *sample(&r, (position >> 16) & 0xfffe);
                    }
                }
                break;
            }
            case A_SETBUFF:
                r.in = (uint16_t) w0;
                r.out = (uint16_t) (w1 >> 16);
                r.count = (uint16_t) w1;
                break;
            case A_DMEMMOVE:
                dmem_move(&r, (uint16_t) (w1 >> 16), (uint16_t) w0, (uint16_t) w1);
                break;
            case A_LOADADPCM: {
                const uint32_t n = w0 & 0xffff;
                memcpy(r.book, (const void *) w1, n < sizeof(r.book) ? n : sizeof(r.book));
                break;
            }
            case A_MIXER: {
                // out * 0x7fff + in * gain, as Q15 products, rounded.
                const int32_t gain = (int16_t) w0;
                const uint16_t in = (uint16_t) (w1 >> 16), out = (uint16_t) w1;
                for (int32_t n = (w0 >> 12) & 0xff0, at = 0; n > 0; n -= 32) {
                    for (int i = 0; i < 16; ++i, at += 2) {
                        const int32_t acc = (*sample(&r, out + at) * 0x7fff + *sample(&r, in + at) * gain) * 2 + 0x8000;
                        *sample(&r, out + at) = clamp16(acc >> 16);
                    }
                }
                break;
            }
            case A_INTERLEAVE: {
                const uint16_t left = (uint16_t) (w1 >> 16), right = (uint16_t) w1;
                uint16_t out = (uint16_t) w0;
                for (int32_t n = (w0 >> 12) & 0xff0, at = 0; n > 0; n -= 8) {
                    for (int i = 0; i < 4; ++i, at += 2, out += 4) {
                        const int16_t l = *sample(&r, left + at), rr = *sample(&r, right + at);
                        *sample(&r, out) = l;
                        *sample(&r, out + 2) = rr;
                    }
                }
                break;
            }
            case A_SETLOOP:
                r.loop = (int16_t *) w1;
                break;
            case A_DMEMMOVE2: {
                const uint16_t from = (uint16_t) w0, to = (uint16_t) (w1 >> 16);
                uint32_t at = 0;
                for (int t = (w0 >> 16) & 0xff; t > 0; --t) {
                    for (int32_t n = w1 & 0xffff; n > 0; n -= 32, at += 32) {
                        for (int i = 0; i < 32; ++i) {
                            *byte(&r, to + at + i) = *byte(&r, from + at + i);
                        }
                    }
                }
                break;
            }
            case A_DOWNSAMPLE_HALF: {
                const uint16_t in = (uint16_t) (w1 >> 16), out = (uint16_t) w1;
                uint32_t i = 0;
                for (int32_t n = w0 & 0xffff; n > 0; n -= 8) {
                    for (int k = 0; k < 8; ++k, ++i) {
                        *sample(&r, out + i * 2) = *sample(&r, in + i * 4);
                    }
                }
                break;
            }
            case A_ENVSETUP1:
                r.env[4] = (uint16_t) ((w0 >> 8) & 0xff00);
                r.env[5] = (uint16_t) (r.env[4] + (uint16_t) w0);
                r.ramp[2] = (int16_t) w0;
                r.ramp[0] = (int16_t) (w1 >> 16);
                r.ramp[1] = (int16_t) w1;
                break;
            case A_ENVSETUP2:
                r.env[0] = (uint16_t) (w1 >> 16);
                r.env[1] = (uint16_t) (r.env[0] + r.ramp[0]);
                r.env[2] = (uint16_t) w1;
                r.env[3] = (uint16_t) (r.env[2] + r.ramp[1]);
                break;
            case A_ENVMIXER:
                envmixer(&r, w0, (uint32_t) w1);
                break;
            case A_LOADBUFF:
                load(&r, (uint16_t) w0, (const void *) w1, (w0 >> 12) & 0xff0);
                break;
            case A_SAVEBUFF:
                save(&r, (void *) w1, (uint16_t) w0, (w0 >> 12) & 0xff0);
                break;
            case A_S8DEC:
                s8dec(&r, flags & A_INIT, flags & A_LOOP, r.out, r.in, r.count, (int16_t *) w1);
                break;
            case A_HILOGAIN: {
                // By a UQ4.4 gain.
                const int32_t whole = (w0 >> 20) & 0xf, fraction = (w0 >> 4) & 0xf000;
                const uint16_t at = (uint16_t) (w1 >> 16);
                uint32_t i = 0;
                for (int32_t n = w0 & 0xffff; n > 0; n -= 32) {
                    for (int k = 0; k < 16; ++k, ++i) {
                        const int32_t x = *sample(&r, at + i * 2);
                        *sample(&r, at + i * 2) = clamp16((int32_t) (((int64_t) x * fraction + ((int64_t) x * whole << 16)) >> 16));
                    }
                }
                break;
            }
            case A_UNK_25: {
                // Each sample times the one of a table of 32, as integers.
                const uint16_t at = (uint16_t) (w1 >> 16), table = (uint16_t) (w1 + ((w0 >> 16) & 0xff));
                int16_t factor[32];
                for (int k = 0; k < 32; ++k) {
                    factor[k] = *sample(&r, table + k * 2);
                }
                uint32_t i = 0;
                for (int32_t n = w0 & 0xffff; n > 0; n -= 64) {
                    for (int k = 0; k < 32; ++k, ++i) {
                        *sample(&r, at + i * 2) = clamp16(*sample(&r, at + i * 2) * factor[k]);
                    }
                }
                break;
            }
            case A_DUPLICATE: {
                const uint16_t from = (uint16_t) w0;
                uint16_t to = (uint16_t) (w1 >> 16);
                uint8_t block[128];
                for (int i = 0; i < 128; ++i) {
                    block[i] = *byte(&r, from + i);
                }
                int t = (w0 >> 16) & 0xff;
                do {
                    for (int i = 0; i < 128; ++i) {
                        *byte(&r, to + i) = block[i];
                    }
                    to += 128;
                } while (--t > 0);
                break;
            }
            case A_FILTER:
                if (flags > 1) {
                    r.filter_count = (uint16_t) w0;
                    memcpy(r.filter, (const void *) w1, sizeof(r.filter));
                } else {
                    filter(&r, flags == 1, (uint16_t) w0, (int16_t *) w1);
                }
                break;
            default:
                break;
        }
    }
}

#endif
