#include <ultra64.h>

#include "config.h"
#include "zbuffer.h"

ALIGNED8 u16 gZBuffer[SCREEN_WIDTH * SCREEN_HEIGHT];

// Library: its variables' addresses (tools/state/types.py).
#include "pointers/game/src/buffers/zbuffer.c.inc.c"
