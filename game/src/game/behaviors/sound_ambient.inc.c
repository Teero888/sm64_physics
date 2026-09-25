// sound_ambient.inc.c

void bhv_ambient_sounds_init(void) {
    if (WORLD(gCamera)->mode == CAMERA_MODE_BEHIND_MARIO) {
        return;
    }

    play_sound(SOUND_AIR_CASTLE_OUTDOORS_AMBIENT, WORLD(gGlobalSoundSource));
}
