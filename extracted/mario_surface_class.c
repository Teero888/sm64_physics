/* Generated verbatim upstream function extraction. See extracted.json. */
#include "sm64.h"
#include "game/mario.h"
#include "game/area.h"
#include "engine/math_util.h"

#line 387 "n64decomp/src/game/mario.c"
s32 mario_get_floor_class(struct MarioState *m) {
    s32 floorClass;

    // The slide terrain type defaults to slide slipperiness.
    // This doesn't matter too much since normally the slide terrain
    // is checked for anyways.
    if ((m->area->terrainType & TERRAIN_MASK) == TERRAIN_SLIDE) {
        floorClass = SURFACE_CLASS_VERY_SLIPPERY;
    } else {
        floorClass = SURFACE_CLASS_DEFAULT;
    }

    if (m->floor != NULL) {
        switch (m->floor->type) {
            case SURFACE_NOT_SLIPPERY:
            case SURFACE_HARD_NOT_SLIPPERY:
            case SURFACE_SWITCH:
                floorClass = SURFACE_CLASS_NOT_SLIPPERY;
                break;

            case SURFACE_SLIPPERY:
            case SURFACE_NOISE_SLIPPERY:
            case SURFACE_HARD_SLIPPERY:
            case SURFACE_NO_CAM_COL_SLIPPERY:
                floorClass = SURFACE_CLASS_SLIPPERY;
                break;

            case SURFACE_VERY_SLIPPERY:
            case SURFACE_ICE:
            case SURFACE_HARD_VERY_SLIPPERY:
            case SURFACE_NOISE_VERY_SLIPPERY_73:
            case SURFACE_NOISE_VERY_SLIPPERY_74:
            case SURFACE_NOISE_VERY_SLIPPERY:
            case SURFACE_NO_CAM_COL_VERY_SLIPPERY:
                floorClass = SURFACE_CLASS_VERY_SLIPPERY;
                break;
        }
    }

    // Crawling allows Mario to not slide on certain steeper surfaces.
    if (m->action == ACT_CRAWLING && m->floor->normal.y > 0.5f && floorClass == SURFACE_CLASS_DEFAULT) {
        floorClass = SURFACE_CLASS_NOT_SLIPPERY;
    }

    return floorClass;
}

#line 573 "n64decomp/src/game/mario.c"
u32 mario_floor_is_slippery(struct MarioState *m) {
    f32 normY;

    if ((m->area->terrainType & TERRAIN_MASK) == TERRAIN_SLIDE
        && m->floor->normal.y < 0.9998477f //~cos(1 deg)
    ) {
        return TRUE;
    }

    switch (mario_get_floor_class(m)) {
        case SURFACE_VERY_SLIPPERY:
            normY = 0.9848077f; //~cos(10 deg)
            break;

        case SURFACE_SLIPPERY:
            normY = 0.9396926f; //~cos(20 deg)
            break;

        default:
            normY = 0.7880108f; //~cos(38 deg)
            break;

        case SURFACE_NOT_SLIPPERY:
            normY = 0.0f;
            break;
    }

    return m->floor->normal.y <= normY;
}

#line 556 "n64decomp/src/game/mario.c"
s32 mario_facing_downhill(struct MarioState *m, s32 turnYaw) {
    s16 faceAngleYaw = m->faceAngle[1];

    // This is never used in practice, as turnYaw is
    // always passed as zero.
    if (turnYaw && m->forwardVel < 0.0f) {
        faceAngleYaw += 0x8000;
    }

    faceAngleYaw = m->floorAngle - faceAngleYaw;

    return (-0x4000 < faceAngleYaw) && (faceAngleYaw < 0x4000);
}

#line 374 "n64decomp/src/game/mario.c"
void mario_set_forward_vel(struct MarioState *m, f32 forwardVel) {
    m->forwardVel = forwardVel;

    m->slideVelX = sins(m->faceAngle[1]) * m->forwardVel;
    m->slideVelZ = coss(m->faceAngle[1]) * m->forwardVel;

    m->vel[0] = (f32) m->slideVelX;
    m->vel[2] = (f32) m->slideVelZ;
}
