/* Generated verbatim upstream function extraction. See extracted.json. */
#include "sm64.h"
#include "game/mario.h"
#include "game/mario_step.h"
#include "engine/math_util.h"

#line 763 "n64decomp/src/game/mario.c"
static void set_mario_y_vel_based_on_fspeed(struct MarioState *m, f32 initialVelY, f32 multiplier) {
    // get_additive_y_vel_for_jumps is always 0 and a stubbed function.
    // It was likely trampoline related based on code location.
    m->vel[1] = initialVelY + get_additive_y_vel_for_jumps() + m->forwardVel * multiplier;

    if (m->squishTimer != 0 || m->quicksandDepth > 1.0f) {
        m->vel[1] *= 0.5f;
    }
}

#line 776 "n64decomp/src/game/mario.c"
static u32 set_mario_action_airborne(struct MarioState *m, u32 action, u32 actionArg) {
    f32 forwardVel;

    if ((m->squishTimer != 0 || m->quicksandDepth >= 1.0f)
        && (action == ACT_DOUBLE_JUMP || action == ACT_TWIRLING)) {
        action = ACT_JUMP;
    }

    switch (action) {
        case ACT_DOUBLE_JUMP:
            set_mario_y_vel_based_on_fspeed(m, 52.0f, 0.25f);
            m->forwardVel *= 0.8f;
            break;

        case ACT_BACKFLIP:
            m->marioObj->header.gfx.animInfo.animID = -1;
            m->forwardVel = -16.0f;
            set_mario_y_vel_based_on_fspeed(m, 62.0f, 0.0f);
            break;

        case ACT_TRIPLE_JUMP:
            set_mario_y_vel_based_on_fspeed(m, 69.0f, 0.0f);
            m->forwardVel *= 0.8f;
            break;

        case ACT_FLYING_TRIPLE_JUMP:
            set_mario_y_vel_based_on_fspeed(m, 82.0f, 0.0f);
            break;

        case ACT_WATER_JUMP:
        case ACT_HOLD_WATER_JUMP:
            if (actionArg == 0) {
                set_mario_y_vel_based_on_fspeed(m, 42.0f, 0.0f);
            }
            break;

        case ACT_BURNING_JUMP:
            m->vel[1] = 31.5f;
            m->forwardVel = 8.0f;
            break;

        case ACT_RIDING_SHELL_JUMP:
            set_mario_y_vel_based_on_fspeed(m, 42.0f, 0.25f);
            break;

        case ACT_JUMP:
        case ACT_HOLD_JUMP:
            m->marioObj->header.gfx.animInfo.animID = -1;
            set_mario_y_vel_based_on_fspeed(m, 42.0f, 0.25f);
            m->forwardVel *= 0.8f;
            break;

        case ACT_WALL_KICK_AIR:
        case ACT_TOP_OF_POLE_JUMP:
            set_mario_y_vel_based_on_fspeed(m, 62.0f, 0.0f);
            if (m->forwardVel < 24.0f) {
                m->forwardVel = 24.0f;
            }
            m->wallKickTimer = 0;
            break;

        case ACT_SIDE_FLIP:
            set_mario_y_vel_based_on_fspeed(m, 62.0f, 0.0f);
            m->forwardVel = 8.0f;
            m->faceAngle[1] = m->intendedYaw;
            break;

        case ACT_STEEP_JUMP:
            m->marioObj->header.gfx.animInfo.animID = -1;
            set_mario_y_vel_based_on_fspeed(m, 42.0f, 0.25f);
            m->faceAngle[0] = -0x2000;
            break;

        case ACT_LAVA_BOOST:
            m->vel[1] = 84.0f;
            if (actionArg == 0) {
                m->forwardVel = 0.0f;
            }
            break;

        case ACT_DIVE:
            if ((forwardVel = m->forwardVel + 15.0f) > 48.0f) {
                forwardVel = 48.0f;
            }
            mario_set_forward_vel(m, forwardVel);
            break;

        case ACT_LONG_JUMP:
            m->marioObj->header.gfx.animInfo.animID = -1;
            set_mario_y_vel_based_on_fspeed(m, 30.0f, 0.0f);
            m->marioObj->oMarioLongJumpIsSlow = m->forwardVel > 16.0f ? FALSE : TRUE;

            //! (BLJ's) This properly handles long jumps from getting forward speed with
            //  too much velocity, but misses backwards longs allowing high negative speeds.
            if ((m->forwardVel *= 1.5f) > 48.0f) {
                m->forwardVel = 48.0f;
            }
            break;

        case ACT_SLIDE_KICK:
            m->vel[1] = 12.0f;
            if (m->forwardVel < 32.0f) {
                m->forwardVel = 32.0f;
            }
            break;

        case ACT_JUMP_KICK:
            m->vel[1] = 20.0f;
            break;
    }

    m->peakHeight = m->pos[1];
    m->flags |= MARIO_UNKNOWN_08;

    return action;
}

#line 896 "n64decomp/src/game/mario.c"
static u32 set_mario_action_moving(struct MarioState *m, u32 action, UNUSED u32 actionArg) {
    s16 floorClass = mario_get_floor_class(m);
    f32 forwardVel = m->forwardVel;
    f32 mag = min(m->intendedMag, 8.0f);

    switch (action) {
        case ACT_WALKING:
            if (floorClass != SURFACE_CLASS_VERY_SLIPPERY) {
                if (0.0f <= forwardVel && forwardVel < mag) {
                    m->forwardVel = mag;
                }
            }

            m->marioObj->oMarioWalkingPitch = 0;
            break;

        case ACT_HOLD_WALKING:
            if (0.0f <= forwardVel && forwardVel < mag / 2.0f) {
                m->forwardVel = mag / 2.0f;
            }
            break;

        case ACT_BEGIN_SLIDING:
            if (mario_facing_downhill(m, FALSE)) {
                action = ACT_BUTT_SLIDE;
            } else {
                action = ACT_STOMACH_SLIDE;
            }
            break;

        case ACT_HOLD_BEGIN_SLIDING:
            if (mario_facing_downhill(m, FALSE)) {
                action = ACT_HOLD_BUTT_SLIDE;
            } else {
                action = ACT_HOLD_STOMACH_SLIDE;
            }
            break;
    }

    return action;
}

#line 941 "n64decomp/src/game/mario.c"
static u32 set_mario_action_submerged(struct MarioState *m, u32 action, UNUSED u32 actionArg) {
    if (action == ACT_METAL_WATER_JUMP || action == ACT_HOLD_METAL_WATER_JUMP) {
        m->vel[1] = 32.0f;
    }

    return action;
}

#line 952 "n64decomp/src/game/mario.c"
static u32 set_mario_action_cutscene(struct MarioState *m, u32 action, UNUSED u32 actionArg) {
    switch (action) {
        case ACT_EMERGE_FROM_PIPE:
            m->vel[1] = 52.0f;
            break;

        case ACT_FALL_AFTER_STAR_GRAB:
            mario_set_forward_vel(m, 0.0f);
            break;

        case ACT_SPAWN_SPIN_AIRBORNE:
            mario_set_forward_vel(m, 2.0f);
            break;

        case ACT_SPECIAL_EXIT_AIRBORNE:
        case ACT_SPECIAL_DEATH_EXIT:
            m->vel[1] = 64.0f;
            break;
    }

    return action;
}

#line 979 "n64decomp/src/game/mario.c"
u32 set_mario_action(struct MarioState *m, u32 action, u32 actionArg) {
    switch (action & ACT_GROUP_MASK) {
        case ACT_GROUP_MOVING:
            action = set_mario_action_moving(m, action, actionArg);
            break;

        case ACT_GROUP_AIRBORNE:
            action = set_mario_action_airborne(m, action, actionArg);
            break;

        case ACT_GROUP_SUBMERGED:
            action = set_mario_action_submerged(m, action, actionArg);
            break;

        case ACT_GROUP_CUTSCENE:
            action = set_mario_action_cutscene(m, action, actionArg);
            break;
    }

    // Resets the sound played flags, meaning Mario can play those sound types again.
    m->flags &= ~(MARIO_ACTION_SOUND_PLAYED | MARIO_MARIO_SOUND_PLAYED);

    if (!(m->action & ACT_FLAG_AIR)) {
        m->flags &= ~MARIO_UNKNOWN_18;
    }

    // Initialize the action information.
    m->prevAction = m->action;
    m->action = action;
    m->actionArg = actionArg;
    m->actionState = 0;
    m->actionTimer = 0;

    return TRUE;
}
