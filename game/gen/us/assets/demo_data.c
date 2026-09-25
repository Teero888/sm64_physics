#include "game/memory.h"
#include <stddef.h>

struct DemoInputsObj {
u32 numEntries;
const void *addrPlaceholder;
struct OffsetSizePair entries[7];
u8 bbh[988];
u8 ccm[1320];
u8 hmc[980];
u8 jrb[620];
u8 wf[672];
u8 pss[748];
u8 unused[108];
u8 bitdw[1412];
} gDemoInputs = {
7,
NULL,
{
{offsetof(struct DemoInputsObj, bitdw), sizeof(gDemoInputs.bitdw)},
{offsetof(struct DemoInputsObj, wf), sizeof(gDemoInputs.wf) + 368},
{offsetof(struct DemoInputsObj, ccm), sizeof(gDemoInputs.ccm)},
{offsetof(struct DemoInputsObj, bbh), sizeof(gDemoInputs.bbh)},
{offsetof(struct DemoInputsObj, jrb), sizeof(gDemoInputs.jrb)},
{offsetof(struct DemoInputsObj, hmc), sizeof(gDemoInputs.hmc)},
{offsetof(struct DemoInputsObj, pss), sizeof(gDemoInputs.pss)},
},
{0},
{0},
{0},
{0},
{0},
{0},
{0},
{0},
};

#include <string.h>

// Library: the demo inputs, from the ROM (platform/host.c).
void host_load_demo_inputs(const unsigned char *rom) {
    memcpy(gDemoInputs.bbh, rom + 0x579c60, sizeof(gDemoInputs.bbh));
    memcpy(gDemoInputs.bitdw, rom + 0x57b19c, sizeof(gDemoInputs.bitdw));
    memcpy(gDemoInputs.ccm, rom + 0x57a03c, sizeof(gDemoInputs.ccm));
    memcpy(gDemoInputs.hmc, rom + 0x57a564, sizeof(gDemoInputs.hmc));
    memcpy(gDemoInputs.jrb, rom + 0x57a938, sizeof(gDemoInputs.jrb));
    memcpy(gDemoInputs.pss, rom + 0x57ae44, sizeof(gDemoInputs.pss));
    memcpy(gDemoInputs.unused, rom + 0x57b130, sizeof(gDemoInputs.unused));
    memcpy(gDemoInputs.wf, rom + 0x57aba4, sizeof(gDemoInputs.wf));
}
