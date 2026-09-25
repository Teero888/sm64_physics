// static_checkered_platform.inc.c

void bhv_static_checkered_platform_loop(void) {
    if (WORLD(gDebugInfo)[DEBUG_PAGE_ENEMYINFO][0] == 1) {
        obj_set_angle(o, 0, 0, 0);
        o->oAngleVelPitch = 0;
        o->oAngleVelYaw = 0;
        o->oAngleVelRoll = 0;
    }

    if (WORLD(gDebugInfo)[DEBUG_PAGE_ENEMYINFO][0] == 2) {
        o->oFaceAnglePitch = WORLD(gDebugInfo)[DEBUG_PAGE_ENEMYINFO][1] << 12;
        o->oFaceAngleYaw = WORLD(gDebugInfo)[DEBUG_PAGE_ENEMYINFO][2] << 12;
        o->oFaceAngleRoll = WORLD(gDebugInfo)[DEBUG_PAGE_ENEMYINFO][3] << 12;
    }

    o->oAngleVelPitch = WORLD(gDebugInfo)[DEBUG_PAGE_ENEMYINFO][4];
    o->oAngleVelYaw = WORLD(gDebugInfo)[DEBUG_PAGE_ENEMYINFO][5];
    o->oAngleVelRoll = WORLD(gDebugInfo)[DEBUG_PAGE_ENEMYINFO][6];

    if (WORLD(gDebugInfo)[DEBUG_PAGE_ENEMYINFO][0] == 3) {
        o->oFaceAnglePitch += o->oAngleVelPitch;
        o->oFaceAngleYaw += o->oAngleVelYaw;
        o->oFaceAngleRoll += o->oAngleVelRoll;
    }
}
