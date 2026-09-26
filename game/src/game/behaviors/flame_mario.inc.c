// flame_mario.inc.c

void bhv_black_smoke_upward_loop(void) {
    N64_STACK_FRAME(bhv_black_smoke_upward_loop);
    spawn_object_with_scale(o, MODEL_BURN_SMOKE, bhvBlackSmokeBowser, o->header.gfx.scale[0]);
}

void bhv_black_smoke_bowser_loop(void) {
    if (o->oTimer == 0) {
        o->oForwardVel = random_float() * 2 + 0.5;
        o->oMoveAngleYaw = random_u16();
        o->oVelY = 8.0f;

        o->oBlackSmokeBowserUnkF4 = o->header.gfx.scale[0];
    }

    o->oMoveAngleYaw += o->oAngleVelYaw;
    o->oPosY += o->oVelY;
}

void bhv_black_smoke_mario_loop(void) {
    if (o->oTimer == 0) {
        cur_obj_set_pos_relative(WORLD(gMarioObject), 0, 0, -30.0f);

        o->oForwardVel = random_float() * 2 + 0.5;
        o->oMoveAngleYaw = (WORLD(gMarioObject)->oMoveAngleYaw + 0x7000) + random_float() * 8192.0f;
        o->oVelY = 8.0f;
    }

    o->oMoveAngleYaw += o->oAngleVelYaw;
    o->oPosY += o->oVelY;
}

void bhv_flame_mario_loop(void) {
    N64_STACK_FRAME(bhv_flame_mario_loop);
    cur_obj_scale(2.0f);

    if (o->oTimer != 0) {
        if (o->oTimer & 1) {
            spawn_object(o, MODEL_BURN_SMOKE, bhvBlackSmokeMario);
        }
    }

    WORLD(gMarioObject)->prevObj = o; // weird?
    obj_set_parent_relative_pos(o, 40, -120, 0);

    if (!(WORLD(gMarioObject)->oMarioParticleFlags & ACTIVE_PARTICLE_FIRE)) {
        o->parentObj->oActiveParticleFlags &= ~ACTIVE_PARTICLE_FIRE;
        obj_mark_for_deletion(o);
        WORLD(gMarioObject)->prevObj = NULL;
    }
}
