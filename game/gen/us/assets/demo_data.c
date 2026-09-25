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
    memcpy(WORLD(gDemoInputs).bbh, rom + 0x579c60, sizeof(WORLD(gDemoInputs).bbh));
    memcpy(WORLD(gDemoInputs).bitdw, rom + 0x57b19c, sizeof(WORLD(gDemoInputs).bitdw));
    memcpy(WORLD(gDemoInputs).ccm, rom + 0x57a03c, sizeof(WORLD(gDemoInputs).ccm));
    memcpy(WORLD(gDemoInputs).hmc, rom + 0x57a564, sizeof(WORLD(gDemoInputs).hmc));
    memcpy(WORLD(gDemoInputs).jrb, rom + 0x57a938, sizeof(WORLD(gDemoInputs).jrb));
    memcpy(WORLD(gDemoInputs).pss, rom + 0x57ae44, sizeof(WORLD(gDemoInputs).pss));
    memcpy(WORLD(gDemoInputs).unused, rom + 0x57b130, sizeof(WORLD(gDemoInputs).unused));
    memcpy(WORLD(gDemoInputs).wf, rom + 0x57aba4, sizeof(WORLD(gDemoInputs).wf));
}

// Library: its variables' addresses (tools/state/types.py).
#include "pointers/game/gen/us/assets/demo_data.c.inc.c"
