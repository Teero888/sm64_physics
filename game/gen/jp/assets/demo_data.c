#include "game/memory.h"
#include <stddef.h>

struct DemoInputsObj {
u32 numEntries;
const void *addrPlaceholder;
struct OffsetSizePair entries[6];
u8 bbh[988];
u8 ccm[1320];
u8 hmc[980];
u8 jrb[620];
u8 wf[672];
u8 pss[748];
u8 unused[108];
} gDemoInputs = {
6,
NULL,
{
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
};

#include <string.h>

// Library: the demo inputs, from the ROM (platform/host.c).
void host_load_demo_inputs(const unsigned char *rom) {
    memcpy(gDemoInputs.bbh, rom + 0x577bf8, sizeof(gDemoInputs.bbh));
    memcpy(gDemoInputs.ccm, rom + 0x577fd4, sizeof(gDemoInputs.ccm));
    memcpy(gDemoInputs.hmc, rom + 0x5784fc, sizeof(gDemoInputs.hmc));
    memcpy(gDemoInputs.jrb, rom + 0x5788d0, sizeof(gDemoInputs.jrb));
    memcpy(gDemoInputs.pss, rom + 0x578ddc, sizeof(gDemoInputs.pss));
    memcpy(gDemoInputs.unused, rom + 0x5790c8, sizeof(gDemoInputs.unused));
    memcpy(gDemoInputs.wf, rom + 0x578b3c, sizeof(gDemoInputs.wf));
}
