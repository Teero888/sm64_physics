// thi_top.inc.c

static struct SpawnParticlesInfo sTHITopPuffs = {
    /* bhvParam:        */ 0,
    /* count:           */ 30,
    /* model:           */ MODEL_WHITE_PARTICLE_SMALL,
    /* offsetY:         */ 0,
    /* forwardVelBase:  */ 40,
    /* forwardVelRange: */ 0,
    /* velYBase:        */ 20,
    /* velYRange:       */ 40,
    /* gravity:         */ 252,
    /* dragStrength:    */ 30,
    /* sizeBase:        */ 20.0f,
    /* sizeRange:       */ 0.0f,
};

void bhv_thi_huge_island_top_loop(void) {
    if (WORLD(gTHIWaterDrained) & 1) {
        if (o->oTimer == 0) {
            WORLD(gEnvironmentRegions)[18] = 3000;
        }
        cur_obj_hide();
    } else {
        load_object_collision_model();
    }
}

void bhv_thi_tiny_island_top_loop(void) {
    N64_STACK_FRAME(bhv_thi_tiny_island_top_loop);
    if (!(WORLD(gTHIWaterDrained) & 1)) {
        if (o->oAction == 0) {
            if (o->oDistanceToMario < 500.0f) {
                if (WORLD(gMarioStates)[0].action == ACT_GROUND_POUND_LAND) {
                    o->oAction++;
                    cur_obj_spawn_particles(&WORLD(sTHITopPuffs));
                    spawn_triangle_break_particles(20, MODEL_DIRT_ANIMATION, 0.3f, 3);
                    cur_obj_play_sound_2(SOUND_GENERAL_ACTIVATE_CAP_SWITCH);
                    cur_obj_hide();
                }
            }
        } else {
            if (o->oTimer < 50) {
                WORLD(gEnvironmentRegions)[18]--;
                cur_obj_play_sound_1(SOUND_ENV_WATER_DRAIN);
            } else {
                WORLD(gTHIWaterDrained) |= 1;
                play_puzzle_jingle();
                o->oAction++;
            }
        }
    } else {
        if (o->oTimer == 0) {
            WORLD(gEnvironmentRegions)[18] = 700;
        }
        cur_obj_hide();
    }
}
