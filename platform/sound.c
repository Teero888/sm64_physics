// The sound banks, sample banks, sequences and bank sets, from the user's ROM
// (sound/sound_data.c in the decomp). Not part of the state: the process
// keeps one converted copy for every world, which the sound thread copies
// from as the N64's reads the ROM.
//
// The ROM holds the banks as the N64's structs: big-endian, with 32-bit
// offsets for pointers. The game copies a bank and turns its offsets into
// pointers in place (patch_audio_bank), so the copy it gets has to be the
// host's structs: the same objects, laid out as the host lays them out, with
// the offsets they refer to each other by. The samples, the sequences and
// the envelopes (which the game reads big-endian itself) are bytes as they
// are.
#include <ultra64.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "audio/internal.h"
#include "sound.h"

// Before the ROM is loaded: empty headers, so the sound thread's setup finds
// no sequences, instruments or samples.
static u8 sEmpty[0x100] __attribute__((aligned(16)));

u8 *gSoundDataADSR = sEmpty; // sound_data.ctl
u8 *gSoundDataRaw = sEmpty;  // sound_data.tbl
u8 *gMusicData = sEmpty;     // sequences
#if !defined(VERSION_SH)
u8 *gBankSetsData = sEmpty; // bank sets
#endif

// Where each version's are in its ROM (the decomp's sound/sound_data.o). The
// Shindou Edition's files hold only the data: their headers and the bank sets
// are the audio loader's (src/audio/load_sh.c), in the ROM's code segment.
struct sound_rom {
    uint32_t ctl, tbl, sequences, bank_sets, end;
    uint32_t ctl_header, tbl_header, sequences_header;
};
#if defined(VERSION_JP)
static const struct sound_rom sSoundRom = { 0x579140, 0x590200, 0x745f80, 0x761b40, 0x761be0 };
#elif defined(VERSION_EU)
static const struct sound_rom sSoundRom = { 0x55d8b0, 0x5756f0, 0x7929f0, 0x7ae930, 0x7ae9d0 };
#elif defined(VERSION_SH)
static const struct sound_rom sSoundRom = { 0x556540, 0x56c2c0, 0x7818b0, 0xd3280, 0x79d6c0,
                                            0xd3010, 0xd3320, 0xd33e0 };
#define BANK_SETS_SIZE 0xa0
#else
static const struct sound_rom sSoundRom = { 0x57b720, 0x593560, 0x7b0860, 0x7cc620, 0x7cc6c0 };
#endif

// --- Reading the ROM --------------------------------------------------------------

struct in {
    const unsigned char *data;
    size_t size;
};

static uint32_t be32(const struct in *in, uint32_t at) {
    if (at + 4 > in->size) {
        return 0;
    }
    const unsigned char *p = in->data + at;
    return (uint32_t) p[0] << 24 | (uint32_t) p[1] << 16 | (uint32_t) p[2] << 8 | p[3];
}

static uint16_t be16(const struct in *in, uint32_t at) {
    if (at + 2 > in->size) {
        return 0;
    }
    return (uint16_t) (in->data[at] << 8 | in->data[at + 1]);
}

static uint8_t byte(const struct in *in, uint32_t at) {
    return at < in->size ? in->data[at] : 0;
}

static f32 be_float(const struct in *in, uint32_t at) {
    const uint32_t bits = be32(in, at);
    f32 f;
    memcpy(&f, &bits, 4);
    return f;
}

// --- Writing the host's objects ----------------------------------------------

struct out {
    unsigned char *data;
    size_t size, capacity;
    bool failed;
};

// Room for size bytes at an alignment: their offset.
static size_t take(struct out *out, size_t size, size_t alignment) {
    size_t at = (out->size + alignment - 1) & ~(alignment - 1);
    if (at + size > out->capacity) {
        size_t capacity = out->capacity ? out->capacity : 0x10000;
        while (at + size > capacity) {
            capacity *= 2;
        }
        unsigned char *grown = realloc(out->data, capacity);
        if (!grown) {
            out->failed = true;
            return 0;
        }
        memset(grown + out->capacity, 0, capacity - out->capacity);
        out->data = grown;
        out->capacity = capacity;
    }
    out->size = at + size;
    return at;
}

#define AT(out, offset, type) ((type *) ((out)->data + (offset)))
#define OFFSET(x) ((void *) (uintptr_t) (x))

// A bank's objects, each converted once: ROM offset -> host offset.
struct converted {
    uint32_t rom, host;
};

struct bank {
    const struct in *in;
    uint32_t base; // the bank's start in the ROM file, which its offsets count from
    struct out *out;
    size_t root;   // the bank's start in the host file
    struct converted *done;
    size_t done_count, done_capacity;
};

static bool lookup(struct bank *b, uint32_t rom, uint32_t *host) {
    for (size_t i = 0; i < b->done_count; ++i) {
        if (b->done[i].rom == rom) {
            *host = b->done[i].host;
            return true;
        }
    }
    return false;
}

static void remember(struct bank *b, uint32_t rom, uint32_t host) {
    if (b->done_count == b->done_capacity) {
        b->done_capacity = b->done_capacity ? b->done_capacity * 2 : 256;
        struct converted *grown = realloc(b->done, b->done_capacity * sizeof(*grown));
        if (!grown) {
            b->out->failed = true;
            return;
        }
        b->done = grown;
    }
    b->done[b->done_count++] = (struct converted) { rom, host };
}

// Where an object of the bank goes in the host's copy, relative to the bank.
static uint32_t place(struct bank *b, size_t size, size_t alignment) {
    return (uint32_t) (take(b->out, size, alignment) - b->root);
}

static uint32_t envelope(struct bank *b, uint32_t rom) {
    uint32_t host;
    if (lookup(b, rom, &host)) {
        return host;
    }
    // (delay, arg) pairs of big-endian s16, up to a delay that is not one.
    uint32_t count = 0;
    while ((int16_t) be16(b->in, b->base + rom + count * 4) > 0 && count < 256) {
        ++count;
    }
    ++count;
    host = place(b, count * 4, 8);
    memcpy(AT(b->out, b->root + host, u8), b->in->data + b->base + rom, count * 4);
    remember(b, rom, host);
    return host;
}

static uint32_t book(struct bank *b, uint32_t rom) {
    uint32_t host;
    if (lookup(b, rom, &host)) {
        return host;
    }
    const s32 order = (s32) be32(b->in, b->base + rom), predictors = (s32) be32(b->in, b->base + rom + 4);
    const uint32_t count = (uint32_t) (8 * order * predictors);
    host = place(b, offsetof(struct AdpcmBook, book) + count * sizeof(s16), 8);
    struct AdpcmBook *out = AT(b->out, b->root + host, struct AdpcmBook);
    out->order = order;
    out->npredictors = predictors;
    for (uint32_t i = 0; i < count; ++i) {
        out->book[i] = (s16) be16(b->in, b->base + rom + 8 + i * 2);
    }
    remember(b, rom, host);
    return host;
}

static uint32_t loop(struct bank *b, uint32_t rom) {
    uint32_t host;
    if (lookup(b, rom, &host)) {
        return host;
    }
    host = place(b, sizeof(struct AdpcmLoop), 8);
    struct AdpcmLoop *out = AT(b->out, b->root + host, struct AdpcmLoop);
    out->start = be32(b->in, b->base + rom);
    out->end = be32(b->in, b->base + rom + 4);
    out->count = be32(b->in, b->base + rom + 8);
    if (out->count != 0) {
        for (int i = 0; i < 16; ++i) {
            out->state[i] = (s16) be16(b->in, b->base + rom + 16 + i * 2);
        }
    }
    remember(b, rom, host);
    return host;
}

static uint32_t sample(struct bank *b, uint32_t rom) {
    uint32_t host;
    if (lookup(b, rom, &host)) {
        return host;
    }
    host = place(b, sizeof(struct AudioBankSample), 8);
    remember(b, rom, host);
    const uint32_t loop_rom = be32(b->in, b->base + rom + 8), book_rom = be32(b->in, b->base + rom + 12);
    const uint32_t loop_host = loop(b, loop_rom), book_host = book(b, book_rom);
    struct AudioBankSample *out = AT(b->out, b->root + host, struct AudioBankSample);
#if defined(VERSION_SH)
    // Bit fields, allocated from the top bit down on the N64.
    const uint32_t bits = be32(b->in, b->base + rom);
    out->codec = bits >> 28;
    out->medium = bits >> 26 & 3;
    out->bit1 = bits >> 25 & 1;
    out->isPatched = bits >> 24 & 1;
    out->size = bits & 0xffffff;
#else
    out->unused = byte(b->in, b->base + rom);
    out->loaded = byte(b->in, b->base + rom + 1);
    out->sampleSize = be32(b->in, b->base + rom + 16);
#endif
    out->sampleAddr = OFFSET(be32(b->in, b->base + rom + 4)); // in the sample bank: bytes as they are
    out->loop = OFFSET(loop_host);
    out->book = OFFSET(book_host);
    return host;
}

// A sound (sample offset, tuning) at ROM offset rom, into the host's at host.
static void sound(struct bank *b, uint32_t rom, size_t host) {
    const uint32_t sample_rom = be32(b->in, b->base + rom);
    const uint32_t sample_host = sample_rom ? sample(b, sample_rom) : 0;
    struct AudioBankSound *out = AT(b->out, host, struct AudioBankSound);
    out->sample = sample_rom ? OFFSET(sample_host) : NULL;
    out->tuning = be_float(b->in, b->base + rom + 4);
}

static uint32_t instrument(struct bank *b, uint32_t rom) {
    uint32_t host;
    if (lookup(b, rom, &host)) {
        return host;
    }
    host = place(b, sizeof(struct Instrument), 8);
    remember(b, rom, host);
    const uint32_t envelope_host = envelope(b, be32(b->in, b->base + rom + 4));
    struct Instrument *out = AT(b->out, b->root + host, struct Instrument);
    out->loaded = byte(b->in, b->base + rom);
    out->normalRangeLo = byte(b->in, b->base + rom + 1);
    out->normalRangeHi = byte(b->in, b->base + rom + 2);
    out->releaseRate = byte(b->in, b->base + rom + 3);
    out->envelope = OFFSET(envelope_host);
    const size_t at = b->root + host;
    sound(b, rom + 8, at + offsetof(struct Instrument, lowNotesSound));
    sound(b, rom + 16, at + offsetof(struct Instrument, normalNotesSound));
    sound(b, rom + 24, at + offsetof(struct Instrument, highNotesSound));
    return host;
}

static uint32_t drum(struct bank *b, uint32_t rom) {
    uint32_t host;
    if (lookup(b, rom, &host)) {
        return host;
    }
    host = place(b, sizeof(struct Drum), 8);
    remember(b, rom, host);
    const uint32_t envelope_host = envelope(b, be32(b->in, b->base + rom + 12));
    struct Drum *out = AT(b->out, b->root + host, struct Drum);
    out->releaseRate = byte(b->in, b->base + rom);
    out->pan = byte(b->in, b->base + rom + 1);
    out->loaded = byte(b->in, b->base + rom + 2);
    out->envelope = OFFSET(envelope_host);
    sound(b, rom + 4, b->root + host + offsetof(struct Drum, sound));
    return host;
}

// One bank of the ctl file (ROM at start) into out: struct AudioBank and what
// it points to. JP, US and EU banks start with a header of 16 bytes, which
// the game reads as numbers (instruments, drums); Shindou's counts are in the
// file's header instead.
static void convert_bank(const struct in *in, uint32_t start, struct out *out, uint32_t instruments,
                         uint32_t drums) {
#if !defined(VERSION_SH)
    instruments = be32(in, start);
    drums = be32(in, start + 4);
    const size_t header = take(out, 16, 16);
    AT(out, header, u32)[0] = instruments;
    AT(out, header, u32)[1] = drums;
    start += 16;
#endif
    struct bank b = { in, start, out, 0, NULL, 0, 0 };
    b.root = take(out, offsetof(struct AudioBank, instruments) + instruments * sizeof(void *), 16);
    const uint32_t drums_rom = be32(in, b.base);
    uint32_t drums_host = 0;
    if (drums_rom != 0 && drums > 0) {
        drums_host = place(&b, drums * sizeof(void *), 8);
        for (uint32_t i = 0; i < drums; ++i) {
            const uint32_t d = be32(in, b.base + drums_rom + i * 4);
            const uint32_t host = d ? drum(&b, d) : 0;
            AT(out, b.root + drums_host, void *)[i] = d ? OFFSET(host) : NULL;
        }
    }
    AT(out, b.root, struct AudioBank)->drums = drums_rom ? OFFSET(drums_host) : NULL;
    for (uint32_t i = 0; i < instruments; ++i) {
        const uint32_t r = be32(in, b.base + 4 + i * 4);
        const uint32_t host = r ? instrument(&b, r) : 0;
        AT(out, b.root, struct AudioBank)->instruments[i] = r ? OFFSET(host) : NULL;
    }
    free(b.done);
    take(out, 0, 16);
}

static bool copy(const struct in *in, uint32_t from, uint32_t size, struct out *out) {
    const size_t at = take(out, size, 16);
    if (!out->failed && from + size <= in->size) {
        memcpy(out->data + at, in->data + from, size);
    }
    return !out->failed;
}

static struct out sCtl, sTbl, sSequences, sBankSets;

#if !defined(VERSION_SH)

// A file of the ROM's (ALSeqFile: revision, count, then offset and length of
// each entry) as the host's ALSeqFile, with each entry's data after it: the
// banks converted, the rest as they are.
static bool convert_file(const struct in *in, uint32_t file, struct out *out, bool banks) {
    const uint16_t revision = be16(in, file), count = be16(in, file + 2);
    const size_t header = take(out, offsetof(ALSeqFile, seqArray) + count * sizeof(ALSeqData), 16);
    AT(out, header, ALSeqFile)->revision = (s16) revision;
    AT(out, header, ALSeqFile)->seqCount = (s16) count;
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t offset = be32(in, file + 4 + i * 8), length = be32(in, file + 8 + i * 8);
        const size_t at = take(out, 0, 16);
        if (banks && length != 0) {
            convert_bank(in, file + offset, out, 0, 0);
        } else {
            copy(in, file + offset, length, out);
        }
        AT(out, header, ALSeqFile)->seqArray[i].offset = OFFSET(at);
        AT(out, header, ALSeqFile)->seqArray[i].len = (s32) (out->size - at);
    }
    return !out->failed;
}

bool host_load_sound(const unsigned char *rom, size_t size) {
    const struct in in = { rom, size };
    if (sSoundRom.end > size) {
        return false;
    }
    struct out ctl = { 0 }, tbl = { 0 }, sequences = { 0 }, bank_sets = { 0 };
    bool ok = convert_file(&in, sSoundRom.ctl, &ctl, true) && convert_file(&in, sSoundRom.tbl, &tbl, false)
              && convert_file(&in, sSoundRom.sequences, &sequences, false);
    // Bank sets: a u16 offset per sequence, then bytes. The game reads 0x100.
    const uint32_t sets = sSoundRom.end - sSoundRom.bank_sets;
    ok = ok && copy(&in, sSoundRom.bank_sets, sets > 0x100 ? sets : 0x100, &bank_sets);
    if (ok) {
        const uint32_t tables = be16(&in, sSoundRom.bank_sets) / 2;
        for (uint32_t i = 0; i < tables && i * 2 + 1 < sets; ++i) {
            AT(&bank_sets, i * 2, u16)[0] = be16(&in, sSoundRom.bank_sets + i * 2);
        }
    }
    if (!ok) {
        free(ctl.data), free(tbl.data), free(sequences.data), free(bank_sets.data);
        return false;
    }
    // Replacing what an earlier ROM loaded; worlds made before keep pointers
    // into it, so it stays.
    sCtl = ctl, sTbl = tbl, sSequences = sequences, sBankSets = bank_sets;
    gSoundDataADSR = sCtl.data;
    gSoundDataRaw = sTbl.data;
    gMusicData = sSequences.data;
    gBankSetsData = sBankSets.data;
    return true;
}

#else

// src/audio/load_sh.c: the headers, as the host's ALSeqFile (tools/rom_stubs.py
// sizes them), and the bank sets.
extern u8 gShindouSoundBanksHeader[1024], gShindouSampleBanksHeader[512], gShindouSequencesHeader[1024];
extern u8 gBankSetsData[256];

// A header of the ROM's (count, then offset, length, medium, magic and, for
// banks, bank and instrument numbers of each entry) into the world's array
// of the host's; with convert, each entry's data converted into out, which
// becomes where its offsets count from. Entries of length 0 name the entry
// they stand for instead of an offset.
static bool convert_header(const struct in *in, uint32_t header, uint32_t data, u8 *to, size_t room,
                           struct out *out) {
    const uint16_t count = be16(in, header);
    if (offsetof(ALSeqFile, seqArray) + count * sizeof(ALSeqData) > room) {
        return false;
    }
    memset(to, 0, room);
    ALSeqFile *file = (ALSeqFile *) to;
    file->seqCount = (s16) count;
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t e = header + 16 + i * 16;
        const uint32_t offset = be32(in, e), length = be32(in, e + 4);
        ALSeqData *entry = &file->seqArray[i];
        entry->medium = (s8) byte(in, e + 8);
        entry->magic = (s8) byte(in, e + 9);
        entry->ctl.as_s16.bankAndFf = (s16) be16(in, e + 10);
        entry->ctl.as_s16.numInstrumentsAndDrums = (s16) be16(in, e + 12);
        entry->offset = OFFSET(offset);
        entry->len = (s32) length;
        if (out != NULL && length != 0) {
            const size_t at = take(out, 0, 16);
            convert_bank(in, data + offset, out, byte(in, e + 12), byte(in, e + 13));
            entry->offset = OFFSET(at);
            entry->len = (s32) (out->size - at);
        }
    }
    return !out || !out->failed;
}

// The data into the process's copies, the headers and bank sets into the
// current world (sm64_load_rom makes it the initial values).
bool host_load_sound(const unsigned char *rom, size_t size) {
    const struct in in = { rom, size };
    if (sSoundRom.end > size) {
        return false;
    }
    struct out ctl = { 0 }, tbl = { 0 }, sequences = { 0 };
    bool ok = convert_header(&in, sSoundRom.ctl_header, sSoundRom.ctl, WORLD(gShindouSoundBanksHeader),
                             sizeof(gShindouSoundBanksHeader), &ctl)
              && convert_header(&in, sSoundRom.tbl_header, 0, WORLD(gShindouSampleBanksHeader),
                                sizeof(gShindouSampleBanksHeader), NULL)
              && convert_header(&in, sSoundRom.sequences_header, 0, WORLD(gShindouSequencesHeader),
                                sizeof(gShindouSequencesHeader), NULL)
              && copy(&in, sSoundRom.tbl, sSoundRom.sequences - sSoundRom.tbl, &tbl)
              && copy(&in, sSoundRom.sequences, sSoundRom.end - sSoundRom.sequences, &sequences);
    if (ok) {
        // A u16 offset per sequence, then bytes.
        memset(WORLD(gBankSetsData), 0, sizeof(gBankSetsData));
        memcpy(WORLD(gBankSetsData), rom + sSoundRom.bank_sets, BANK_SETS_SIZE);
        const uint32_t tables = be16(&in, sSoundRom.bank_sets) / 2;
        for (uint32_t i = 0; i < tables && i * 2 + 1 < BANK_SETS_SIZE; ++i) {
            ((u16 *) WORLD(gBankSetsData))[i] = be16(&in, sSoundRom.bank_sets + i * 2);
        }
    }
    if (!ok) {
        free(ctl.data), free(tbl.data), free(sequences.data);
        return false;
    }
    sCtl = ctl, sTbl = tbl, sSequences = sequences;
    gSoundDataADSR = sCtl.data;
    gSoundDataRaw = sTbl.data;
    gMusicData = sSequences.data;
    return true;
}

#endif
