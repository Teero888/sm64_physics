/* Generated verbatim upstream function extraction. See extracted.json. */
#include "host/terrain_state.h"
#include "engine/math_util.h"

#line 258 "n64decomp/src/game/mario_step.c"
static s32 perform_ground_quarter_step(struct MarioState *m, Vec3f nextPos) {
    UNUSED struct Surface *lowerWall;
    struct Surface *upperWall;
    struct Surface *ceil;
    struct Surface *floor;
    f32 ceilHeight;
    f32 floorHeight;
    f32 waterLevel;

    lowerWall = resolve_and_return_wall_collisions(nextPos, 30.0f, 24.0f);
    upperWall = resolve_and_return_wall_collisions(nextPos, 60.0f, 50.0f);

    floorHeight = find_floor(nextPos[0], nextPos[1], nextPos[2], &floor);
    ceilHeight = vec3f_find_ceil(nextPos, floorHeight, &ceil);

    waterLevel = find_water_level(nextPos[0], nextPos[2]);

    m->wall = upperWall;

    if (floor == NULL) {
        return GROUND_STEP_HIT_WALL_STOP_QSTEPS;
    }

    if ((m->action & ACT_FLAG_RIDING_SHELL) && floorHeight < waterLevel) {
        floorHeight = waterLevel;
        floor = &gWaterSurfacePseudoFloor;
        floor->originOffset = floorHeight; //! Wrong origin offset (no effect)
    }

    if (nextPos[1] > floorHeight + 100.0f) {
        if (nextPos[1] + 160.0f >= ceilHeight) {
            return GROUND_STEP_HIT_WALL_STOP_QSTEPS;
        }

        vec3f_copy(m->pos, nextPos);
        m->floor = floor;
        m->floorHeight = floorHeight;
        return GROUND_STEP_LEFT_GROUND;
    }

    if (floorHeight + 160.0f >= ceilHeight) {
        return GROUND_STEP_HIT_WALL_STOP_QSTEPS;
    }

    vec3f_set(m->pos, nextPos[0], floorHeight, nextPos[2]);
    m->floor = floor;
    m->floorHeight = floorHeight;

    if (upperWall != NULL) {
        s16 wallDYaw = atan2s(upperWall->normal.z, upperWall->normal.x) - m->faceAngle[1];

        if (wallDYaw >= 0x2AAA && wallDYaw <= 0x5555) {
            return GROUND_STEP_NONE;
        }
        if (wallDYaw <= -0x2AAA && wallDYaw >= -0x5555) {
            return GROUND_STEP_NONE;
        }

        return GROUND_STEP_HIT_WALL_CONTINUE_QSTEPS;
    }

    return GROUND_STEP_NONE;
}

#line 322 "n64decomp/src/game/mario_step.c"
s32 perform_ground_step(struct MarioState *m) {
    s32 i;
    u32 stepResult;
    Vec3f intendedPos;

    for (i = 0; i < 4; i++) {
        intendedPos[0] = m->pos[0] + m->floor->normal.y * (m->vel[0] / 4.0f);
        intendedPos[2] = m->pos[2] + m->floor->normal.y * (m->vel[2] / 4.0f);
        intendedPos[1] = m->pos[1];

        stepResult = perform_ground_quarter_step(m, intendedPos);
        if (stepResult == GROUND_STEP_LEFT_GROUND || stepResult == GROUND_STEP_HIT_WALL_STOP_QSTEPS) {
            break;
        }
    }

    m->terrainSoundAddend = mario_get_terrain_sound_addend(m);
    vec3f_copy(m->marioObj->header.gfx.pos, m->pos);
    vec3s_set(m->marioObj->header.gfx.angle, 0, m->faceAngle[1], 0);

    if (stepResult == GROUND_STEP_HIT_WALL_CONTINUE_QSTEPS) {
        stepResult = GROUND_STEP_HIT_WALL;
    }
    return stepResult;
}

#line 348 "n64decomp/src/game/mario_step.c"
u32 check_ledge_grab(struct MarioState *m, struct Surface *wall, Vec3f intendedPos, Vec3f nextPos) {
    struct Surface *ledgeFloor;
    Vec3f ledgePos;
    f32 displacementX;
    f32 displacementZ;

    if (m->vel[1] > 0) {
        return FALSE;
    }

    displacementX = nextPos[0] - intendedPos[0];
    displacementZ = nextPos[2] - intendedPos[2];

    // Only ledge grab if the wall displaced Mario in the opposite direction of
    // his velocity.
    if (displacementX * m->vel[0] + displacementZ * m->vel[2] > 0.0f) {
        return FALSE;
    }

    //! Since the search for floors starts at y + 160, we will sometimes grab
    // a higher ledge than expected (glitchy ledge grab)
    ledgePos[0] = nextPos[0] - wall->normal.x * 60.0f;
    ledgePos[2] = nextPos[2] - wall->normal.z * 60.0f;
    ledgePos[1] = find_floor(ledgePos[0], nextPos[1] + 160.0f, ledgePos[2], &ledgeFloor);

    if (ledgePos[1] - nextPos[1] <= 100.0f) {
        return FALSE;
    }

    vec3f_copy(m->pos, ledgePos);
    m->floor = ledgeFloor;
    m->floorHeight = ledgePos[1];

    m->floorAngle = atan2s(ledgeFloor->normal.z, ledgeFloor->normal.x);

    m->faceAngle[0] = 0;
    m->faceAngle[1] = atan2s(wall->normal.z, wall->normal.x) + 0x8000;
    return TRUE;
}

#line 388 "n64decomp/src/game/mario_step.c"
s32 perform_air_quarter_step(struct MarioState *m, Vec3f intendedPos, u32 stepArg) {
    s16 wallDYaw;
    Vec3f nextPos;
    struct Surface *upperWall;
    struct Surface *lowerWall;
    struct Surface *ceil;
    struct Surface *floor;
    f32 ceilHeight;
    f32 floorHeight;
    f32 waterLevel;

    vec3f_copy(nextPos, intendedPos);

    upperWall = resolve_and_return_wall_collisions(nextPos, 150.0f, 50.0f);
    lowerWall = resolve_and_return_wall_collisions(nextPos, 30.0f, 50.0f);

    floorHeight = find_floor(nextPos[0], nextPos[1], nextPos[2], &floor);
    ceilHeight = vec3f_find_ceil(nextPos, floorHeight, &ceil);

    waterLevel = find_water_level(nextPos[0], nextPos[2]);

    m->wall = NULL;

    //! The water pseudo floor is not referenced when your intended qstep is
    // out of bounds, so it won't detect you as landing.

    if (floor == NULL) {
        if (nextPos[1] <= m->floorHeight) {
            m->pos[1] = m->floorHeight;
            return AIR_STEP_LANDED;
        }

        m->pos[1] = nextPos[1];
        return AIR_STEP_HIT_WALL;
    }

    if ((m->action & ACT_FLAG_RIDING_SHELL) && floorHeight < waterLevel) {
        floorHeight = waterLevel;
        floor = &gWaterSurfacePseudoFloor;
        floor->originOffset = floorHeight; //! Incorrect origin offset (no effect)
    }

    //! This check uses f32, but findFloor uses short (overflow jumps)
    if (nextPos[1] <= floorHeight) {
        if (ceilHeight - floorHeight > 160.0f) {
            m->pos[0] = nextPos[0];
            m->pos[2] = nextPos[2];
            m->floor = floor;
            m->floorHeight = floorHeight;
        }

        //! When ceilHeight - floorHeight <= 160, the step result says that
        // Mario landed, but his movement is cancelled and his referenced floor
        // isn't updated (pedro spots)
        m->pos[1] = floorHeight;
        return AIR_STEP_LANDED;
    }

    if (nextPos[1] + 160.0f > ceilHeight) {
        if (m->vel[1] >= 0.0f) {
            m->vel[1] = 0.0f;

            //! Uses referenced ceiling instead of ceil (ceiling hang upwarp)
            if ((stepArg & AIR_STEP_CHECK_HANG) && m->ceil != NULL
                && m->ceil->type == SURFACE_HANGABLE) {
                return AIR_STEP_GRABBED_CEILING;
            }

            return AIR_STEP_NONE;
        }

        //! Potential subframe downwarp->upwarp?
        if (nextPos[1] <= m->floorHeight) {
            m->pos[1] = m->floorHeight;
            return AIR_STEP_LANDED;
        }

        m->pos[1] = nextPos[1];
        return AIR_STEP_HIT_WALL;
    }

    //! When the wall is not completely vertical or there is a slight wall
    // misalignment, you can activate these conditions in unexpected situations
    if ((stepArg & AIR_STEP_CHECK_LEDGE_GRAB) && upperWall == NULL && lowerWall != NULL) {
        if (check_ledge_grab(m, lowerWall, intendedPos, nextPos)) {
            return AIR_STEP_GRABBED_LEDGE;
        }

        vec3f_copy(m->pos, nextPos);
        m->floor = floor;
        m->floorHeight = floorHeight;
        return AIR_STEP_NONE;
    }

    vec3f_copy(m->pos, nextPos);
    m->floor = floor;
    m->floorHeight = floorHeight;

    if (upperWall != NULL || lowerWall != NULL) {
        m->wall = upperWall != NULL ? upperWall : lowerWall;
        wallDYaw = atan2s(m->wall->normal.z, m->wall->normal.x) - m->faceAngle[1];

        if (m->wall->type == SURFACE_BURNING) {
            return AIR_STEP_HIT_LAVA_WALL;
        }

        if (wallDYaw < -0x6000 || wallDYaw > 0x6000) {
            m->flags |= MARIO_UNKNOWN_30;
            return AIR_STEP_HIT_WALL;
        }
    }

    return AIR_STEP_NONE;
}

#line 583 "n64decomp/src/game/mario_step.c"
void apply_vertical_wind(struct MarioState *m) {
    f32 maxVelY;
    f32 offsetY;

    if (m->action != ACT_GROUND_POUND) {
        offsetY = m->pos[1] - -1500.0f;

        if (m->floor->type == SURFACE_VERTICAL_WIND && -3000.0f < offsetY && offsetY < 2000.0f) {
            if (offsetY >= 0.0f) {
                maxVelY = 10000.0f / (offsetY + 200.0f);
            } else {
                maxVelY = 50.0f;
            }

            if (m->vel[1] < maxVelY) {
                if ((m->vel[1] += maxVelY / 8.0f) > maxVelY) {
                    m->vel[1] = maxVelY;
                }
            }

#ifdef VERSION_JP
            play_sound(SOUND_ENV_WIND2, m->marioObj->header.gfx.cameraToObject);
#endif
        }
    }
}

#line 610 "n64decomp/src/game/mario_step.c"
s32 perform_air_step(struct MarioState *m, u32 stepArg) {
    Vec3f intendedPos;
    s32 i;
    s32 quarterStepResult;
    s32 stepResult = AIR_STEP_NONE;

    m->wall = NULL;

    for (i = 0; i < 4; i++) {
        intendedPos[0] = m->pos[0] + m->vel[0] / 4.0f;
        intendedPos[1] = m->pos[1] + m->vel[1] / 4.0f;
        intendedPos[2] = m->pos[2] + m->vel[2] / 4.0f;

        quarterStepResult = perform_air_quarter_step(m, intendedPos, stepArg);

        //! On one qf, hit OOB/ceil/wall to store the 2 return value, and continue
        // getting 0s until your last qf. Graze a wall on your last qf, and it will
        // return the stored 2 with a sharply angled reference wall. (some gwks)

        if (quarterStepResult != AIR_STEP_NONE) {
            stepResult = quarterStepResult;
        }

        if (quarterStepResult == AIR_STEP_LANDED || quarterStepResult == AIR_STEP_GRABBED_LEDGE
            || quarterStepResult == AIR_STEP_GRABBED_CEILING
            || quarterStepResult == AIR_STEP_HIT_LAVA_WALL) {
            break;
        }
    }

    if (m->vel[1] >= 0.0f) {
        m->peakHeight = m->pos[1];
    }

    m->terrainSoundAddend = mario_get_terrain_sound_addend(m);

    if (m->action != ACT_FLYING) {
        apply_gravity(m);
    }
    apply_vertical_wind(m);

    vec3f_copy(m->marioObj->header.gfx.pos, m->pos);
    vec3s_set(m->marioObj->header.gfx.angle, 0, m->faceAngle[1], 0);

    return stepResult;
}

#line 503 "n64decomp/src/game/mario_step.c"
void apply_twirl_gravity(struct MarioState *m) {
    f32 terminalVelocity;
    f32 heaviness = 1.0f;

    if (m->angleVel[1] > 1024) {
        heaviness = 1024.0f / m->angleVel[1];
    }

    terminalVelocity = -75.0f * heaviness;

    m->vel[1] -= 4.0f * heaviness;
    if (m->vel[1] < terminalVelocity) {
        m->vel[1] = terminalVelocity;
    }
}

#line 519 "n64decomp/src/game/mario_step.c"
u32 should_strengthen_gravity_for_jump_ascent(struct MarioState *m) {
    if (!(m->flags & MARIO_UNKNOWN_08)) {
        return FALSE;
    }

    if (m->action & (ACT_FLAG_INTANGIBLE | ACT_FLAG_INVULNERABLE)) {
        return FALSE;
    }

    if (!(m->input & INPUT_A_DOWN) && m->vel[1] > 20.0f) {
        return (m->action & ACT_FLAG_CONTROL_JUMP_HEIGHT) != 0;
    }

    return FALSE;
}

#line 535 "n64decomp/src/game/mario_step.c"
void apply_gravity(struct MarioState *m) {
    if (m->action == ACT_TWIRLING && m->vel[1] < 0.0f) {
        apply_twirl_gravity(m);
    } else if (m->action == ACT_SHOT_FROM_CANNON) {
        m->vel[1] -= 1.0f;
        if (m->vel[1] < -75.0f) {
            m->vel[1] = -75.0f;
        }
    } else if (m->action == ACT_LONG_JUMP || m->action == ACT_SLIDE_KICK
               || m->action == ACT_BBH_ENTER_SPIN) {
        m->vel[1] -= 2.0f;
        if (m->vel[1] < -75.0f) {
            m->vel[1] = -75.0f;
        }
    } else if (m->action == ACT_LAVA_BOOST || m->action == ACT_FALL_AFTER_STAR_GRAB) {
        m->vel[1] -= 3.2f;
        if (m->vel[1] < -65.0f) {
            m->vel[1] = -65.0f;
        }
    } else if (m->action == ACT_GETTING_BLOWN) {
        m->vel[1] -= m->gettingBlownGravity;
        if (m->vel[1] < -75.0f) {
            m->vel[1] = -75.0f;
        }
    } else if (should_strengthen_gravity_for_jump_ascent(m)) {
        m->vel[1] /= 4.0f;
    } else if (m->action & ACT_FLAG_METAL_WATER) {
        m->vel[1] -= 1.6f;
        if (m->vel[1] < -16.0f) {
            m->vel[1] = -16.0f;
        }
    } else if ((m->flags & MARIO_WING_CAP) && m->vel[1] < 0.0f && (m->input & INPUT_A_DOWN)) {
        m->marioBodyState->wingFlutter = TRUE;

        m->vel[1] -= 2.0f;
        if (m->vel[1] < -37.5f) {
            if ((m->vel[1] += 4.0f) > -37.5f) {
                m->vel[1] = -37.5f;
            }
        }
    } else {
        m->vel[1] -= 4.0f;
        if (m->vel[1] < -75.0f) {
            m->vel[1] = -75.0f;
        }
    }
}

#line 659 "n64decomp/src/game/mario_step.c"
void set_vel_from_pitch_and_yaw(struct MarioState *m) {
    m->vel[0] = m->forwardVel * coss(m->faceAngle[0]) * sins(m->faceAngle[1]);
    m->vel[1] = m->forwardVel * sins(m->faceAngle[0]);
    m->vel[2] = m->forwardVel * coss(m->faceAngle[0]) * coss(m->faceAngle[1]);
}

#line 665 "n64decomp/src/game/mario_step.c"
void set_vel_from_yaw(struct MarioState *m) {
    m->vel[0] = m->slideVelX = m->forwardVel * sins(m->faceAngle[1]);
    m->vel[1] = 0.0f;
    m->vel[2] = m->slideVelZ = m->forwardVel * coss(m->faceAngle[1]);
}
