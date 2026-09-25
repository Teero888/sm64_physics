// The sound banks and sequences (sound/sound_data.c in the decomp) come from
// the ROM and are not part of the library yet: empty headers, so the sound
// thread's setup finds no sequences, instruments or samples. The game does not
// read anything back from the sound thread.
const unsigned char gSoundDataADSR[0x100];
const unsigned char gSoundDataRaw[0x100];
const unsigned char gMusicData[0x100];
#ifndef VERSION_SH // src/audio/load_sh.c has its own
const unsigned char gBankSetsData[0x100];
#endif

// Library: its variables' addresses (tools/state/types.py).
#include "pointers/platform/sound_data.c.inc.c"
