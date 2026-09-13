/* Generated verbatim upstream function extraction. See extracted.json. */
#include "host/terrain_state.h"
#include "engine/math_util.h"

#line 1314 "n64decomp/src/game/mario.c"
void update_mario_geometry_inputs(struct MarioState *m) {
    f32 gasLevel;
    f32 ceilToFloorDist;

    f32_find_wall_collision(&m->pos[0], &m->pos[1], &m->pos[2], 60.0f, 50.0f);
    f32_find_wall_collision(&m->pos[0], &m->pos[1], &m->pos[2], 30.0f, 24.0f);

    m->floorHeight = find_floor(m->pos[0], m->pos[1], m->pos[2], &m->floor);

    // If Mario is OOB, move his position to his graphical position (which was not updated)
    // and check for the floor there.
    // This can cause errant behavior when combined with astral projection,
    // since the graphical position was not Mario's previous location.
    if (m->floor == NULL) {
        vec3f_copy(m->pos, m->marioObj->header.gfx.pos);
        m->floorHeight = find_floor(m->pos[0], m->pos[1], m->pos[2], &m->floor);
    }

    m->ceilHeight = vec3f_find_ceil(m->pos, m->floorHeight, &m->ceil);
    gasLevel = find_poison_gas_level(m->pos[0], m->pos[2]);
    m->waterLevel = find_water_level(m->pos[0], m->pos[2]);

    if (m->floor != NULL) {
        m->floorAngle = atan2s(m->floor->normal.z, m->floor->normal.x);
        m->terrainSoundAddend = mario_get_terrain_sound_addend(m);

        if ((m->pos[1] > m->waterLevel - 40) && mario_floor_is_slippery(m)) {
            m->input |= INPUT_ABOVE_SLIDE;
        }

        if ((m->floor->flags & SURFACE_FLAG_DYNAMIC)
            || (m->ceil && m->ceil->flags & SURFACE_FLAG_DYNAMIC)) {
            ceilToFloorDist = m->ceilHeight - m->floorHeight;

            if ((0.0f <= ceilToFloorDist) && (ceilToFloorDist <= 150.0f)) {
                m->input |= INPUT_SQUISHED;
            }
        }

        if (m->pos[1] > m->floorHeight + 100.0f) {
            m->input |= INPUT_OFF_FLOOR;
        }

        if (m->pos[1] < (m->waterLevel - 10)) {
            m->input |= INPUT_IN_WATER;
        }

        if (m->pos[1] < (gasLevel - 100.0f)) {
            m->input |= INPUT_IN_POISON_GAS;
        }

    } else {
        level_trigger_warp(m, WARP_OP_DEATH);
    }
}

#line 1373 "n64decomp/src/game/mario.c"
void update_mario_inputs(struct MarioState *m) {
    m->particleFlags = 0;
    m->input = 0;
    m->collidedObjInteractTypes = m->marioObj->collidedObjInteractTypes;
    m->flags &= 0xFFFFFF;

    update_mario_button_inputs(m);
    update_mario_joystick_inputs(m);
    update_mario_geometry_inputs(m);

    debug_print_speed_action_normal(m);

    if (gCameraMovementFlags & CAM_MOVE_C_UP_MODE) {
        if (m->action & ACT_FLAG_ALLOW_FIRST_PERSON) {
            m->input |= INPUT_FIRST_PERSON;
        } else {
            gCameraMovementFlags &= ~CAM_MOVE_C_UP_MODE;
        }
    }

    if (!(m->input & (INPUT_NONZERO_ANALOG | INPUT_A_PRESSED))) {
        m->input |= INPUT_UNKNOWN_5;
    }

    // These 3 flags are defined by Bowser stomping attacks
    if (m->marioObj->oInteractStatus
        & (INT_STATUS_MARIO_STUNNED | INT_STATUS_MARIO_KNOCKBACK_DMG | INT_STATUS_MARIO_SHOCKWAVE)) {
        m->input |= INPUT_STOMPED;
    }

    // This function is located near other unused trampoline functions,
    // perhaps logically grouped here with the timers.
    stub_mario_step_1(m);

    if (m->wallKickTimer > 0) {
        m->wallKickTimer--;
    }

    if (m->doubleJumpTimer > 0) {
        m->doubleJumpTimer--;
    }
}
