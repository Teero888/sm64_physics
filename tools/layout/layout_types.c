// Every type the lockstep comparator reads, compiled twice with debug info:
// laid out as on the N64 and as on the host. tools/layout/layout.py reads the
// two layouts back out of DWARF.
#include <ultra64.h>
#include "types.h"
#include "game/area.h"
#include "game/camera.h"
#include "game/level_update.h"
#include "game/save_file.h"
#include "game/game_init.h"
#include "engine/graph_node.h"
#include "engine/surface_collision.h"

struct Object layout_Object;
struct MarioState layout_MarioState;
struct MarioBodyState layout_MarioBodyState;
struct Camera layout_Camera;
struct LakituState layout_LakituState;
struct PlayerCameraState layout_PlayerCameraState;
struct CutsceneVariable layout_CutsceneVariable;
struct Controller layout_Controller;
struct HudDisplay layout_HudDisplay;
struct SaveBuffer layout_SaveBuffer;
struct Surface layout_Surface;
struct WarpDest layout_WarpDest;
struct Area layout_Area;
